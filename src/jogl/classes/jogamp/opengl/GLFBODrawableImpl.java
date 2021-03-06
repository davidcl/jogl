package jogamp.opengl;

import javax.media.nativewindow.NativeSurface;
import javax.media.nativewindow.NativeWindowException;
import javax.media.nativewindow.ProxySurface;
import javax.media.nativewindow.UpstreamSurfaceHook;
import javax.media.opengl.GL;
import javax.media.opengl.GLCapabilities;
import javax.media.opengl.GLCapabilitiesImmutable;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLException;
import javax.media.opengl.GLFBODrawable;

import com.jogamp.nativewindow.MutableGraphicsConfiguration;
import com.jogamp.opengl.FBObject;
import com.jogamp.opengl.FBObject.Attachment;
import com.jogamp.opengl.FBObject.Colorbuffer;
import com.jogamp.opengl.FBObject.TextureAttachment;

/**
 * {@link FBObject} offscreen GLDrawable implementation, i.e. {@link GLFBODrawable}.
 * <p>
 * It utilizes the context lifecycle hook {@link #contextRealized(GLContext, boolean)}
 * to initialize the {@link FBObject} instance.
 * </p>
 * <p>
 * It utilizes the context current hook {@link #contextMadeCurrent(GLContext, boolean) contextMadeCurrent(context, true)} 
 * to {@link FBObject#bind(GL) bind} the FBO.
 * </p>
 * See {@link GLFBODrawable} for double buffering details.
 * 
 * @see GLDrawableImpl#contextRealized(GLContext, boolean)
 * @see GLDrawableImpl#contextMadeCurrent(GLContext, boolean)
 * @see GLDrawableImpl#getDefaultDrawFramebuffer()
 * @see GLDrawableImpl#getDefaultReadFramebuffer()
 */
public class GLFBODrawableImpl extends GLDrawableImpl implements GLFBODrawable {
    protected static final boolean DEBUG = GLDrawableImpl.DEBUG || Debug.debug("FBObject");
    
    private final GLDrawableImpl parent;
    
    private boolean initialized;
    private int texUnit;
    private int samples;
    
    private FBObject[] fbos;
    private int fboIBack;  // points to GL_BACK buffer
    private int fboIFront; // points to GL_FRONT buffer
    private FBObject pendingFBOReset = null;
    private boolean fboBound;
    private static final int bufferCount = 2; // number of FBOs for double buffering. TODO: Possible to configure!
    
    // private DoubleBufferMode doubleBufferMode; // TODO: Add or remove TEXTURE (only) DoubleBufferMode support
    
    private SwapBufferContext swapBufferContext;
    
    public static interface SwapBufferContext {
        public void swapBuffers(boolean doubleBuffered);
    }
    
    protected GLFBODrawableImpl(GLDrawableFactoryImpl factory, GLDrawableImpl parent, NativeSurface surface, 
                                GLCapabilitiesImmutable fboCaps, int textureUnit) {
        super(factory, surface, false);
        this.initialized = false;

        // Replace the chosen caps of dummy-surface w/ it's clone and copied values of orig FBO caps request.
        // The dummy-surface has already been configured, hence value replace is OK
        // and due to cloning, the native GLCapability portion is being preserved. 
        final MutableGraphicsConfiguration msConfig = (MutableGraphicsConfiguration) surface.getGraphicsConfiguration();
        final GLCapabilities fboCapsNative = (GLCapabilities) msConfig.getChosenCapabilities().cloneMutable();
        fboCapsNative.copyFrom(fboCaps);
        msConfig.setChosenCapabilities(fboCapsNative);
            
        this.parent = parent;
        this.texUnit = textureUnit;
        this.samples = fboCaps.getNumSamples();
        
        // default .. // TODO: Add or remove TEXTURE (only) DoubleBufferMode support
        // this.doubleBufferMode = ( samples > 0 || fboCaps.getDoubleBuffered() ) ? DoubleBufferMode.FBO : DoubleBufferMode.NONE ;
        
        this.swapBufferContext = null;
    }
    
    private final void initialize(boolean realize, GL gl) {        
        if(realize) {
            final int maxSamples = gl.getMaxRenderbufferSamples();
            samples = samples <= maxSamples ? samples : maxSamples;
            
            final GLCapabilitiesImmutable caps = (GLCapabilitiesImmutable) surface.getGraphicsConfiguration().getChosenCapabilities();
            final int fbosN;
            if(samples > 0) {
                fbosN = 1;
            } else if( caps.getDoubleBuffered() ) {
                fbosN = bufferCount;
            } else {
                fbosN = 1;
            }

            fbos = new FBObject[fbosN];
            fboIBack = 0;                // head
            fboIFront = fbos.length - 1; // tail
            
            for(int i=0; i<fbosN; i++) {
                fbos[i] = new FBObject();
                fbos[i].reset(gl, getWidth(), getHeight(), samples);
                if(fbos[i].getNumSamples() != samples) {
                    throw new InternalError("Sample number mismatch: "+samples+", fbos["+i+"] "+fbos[i]);
                }
                if(samples > 0) {
                    fbos[i].attachColorbuffer(gl, 0, caps.getAlphaBits()>0);
                } else {
                    fbos[i].attachTexture2D(gl, 0, caps.getAlphaBits()>0);
                }
                if( caps.getStencilBits() > 0 ) {
                    fbos[i].attachRenderbuffer(gl, Attachment.Type.DEPTH_STENCIL, 24);
                } else {
                    fbos[i].attachRenderbuffer(gl, Attachment.Type.DEPTH, 24);
                }
            }
            fbos[fboIFront].syncFramebuffer(gl);
            fboBound = false;
            final GLCapabilities fboCapsNative = (GLCapabilities) surface.getGraphicsConfiguration().getChosenCapabilities();
            fbos[0].formatToGLCapabilities(fboCapsNative);
            fboCapsNative.setDoubleBuffered( fboCapsNative.getDoubleBuffered() || samples > 0 );
            
            initialized = true;            
        } else {
            initialized = false;
            
            for(int i=0; i<fbos.length; i++) {
                fbos[i].destroy(gl);
            }
            fbos=null;
            fboBound = false;   
            pendingFBOReset = null;
        }
        if(DEBUG) {
            System.err.println("GLFBODrawableImpl.initialize("+realize+"): "+this);
            Thread.dumpStack();
        }
    }
    
    public final void setSwapBufferContext(SwapBufferContext sbc) {
        swapBufferContext = sbc;
    }

    private static final void reset(GL gl, FBObject fbo, int fboIdx, int width, int height, int samples) {
        fbo.reset(gl, width, height, samples); // implicit glClear(..)
        if(fbo.getNumSamples() != samples) {
            throw new InternalError("Sample number mismatch: "+samples+", fbos["+fboIdx+"] "+fbo);
        }        
    }
    
    private final void reset(GL gl, int newSamples) throws GLException {
        if(!initialized) {
            // NOP if not yet initializes
            return;
        }
                
        final GLContext curContext = GLContext.getCurrent();
        final GLContext ourContext = gl.getContext();
        final boolean ctxSwitch = null != curContext && curContext != ourContext; 
        if(DEBUG) {
            System.err.println("GLFBODrawableImpl.reset(newSamples "+newSamples+"): BEGIN - ctxSwitch "+ctxSwitch+", "+this);
            Thread.dumpStack();
        }
        Throwable tFBO = null;
        Throwable tGL = null;
        ourContext.makeCurrent();
        fboBound = false; // clear bound-flag immediatly, caused by contextMadeCurrent(..) - otherwise we would swap @ release
        try {                        
            final int maxSamples = gl.getMaxRenderbufferSamples();        
            newSamples = newSamples <= maxSamples ? newSamples : maxSamples;
            
            if(0==samples && 0<newSamples || 0<samples && 0==newSamples) {
                // MSAA on/off switch
                if(DEBUG) {
                    System.err.println("GLFBODrawableImpl.reset(): samples [on/off] reconfig: "+samples+" -> "+newSamples);
                }
                initialize(false, gl);
                samples = newSamples;
                initialize(true, gl);
            } else {            
                if(DEBUG) {
                    System.err.println("GLFBODrawableImpl.reset(): simple reconfig: "+samples+" -> "+newSamples);
                }
                final int nWidth = getWidth();
                final int nHeight = getHeight();
                samples = newSamples;
                pendingFBOReset = ( 1 < fbos.length ) ? fbos[fboIFront] : null; // pending-front reset only w/ double buffering (or zero samples)
                for(int i=0; i<fbos.length; i++) {
                    if(1 == fbos.length || fboIFront != i) {
                        reset(gl, fbos[i], i, nWidth, nHeight, samples);
                    }
                }
                final GLCapabilities fboCapsNative = (GLCapabilities) surface.getGraphicsConfiguration().getChosenCapabilities();
                fbos[0].formatToGLCapabilities(fboCapsNative);
            }
        } catch (Throwable t) {
            tFBO = t;
        } finally {
            try {
                ourContext.release();
                if(ctxSwitch) {
                    curContext.makeCurrent();
                }
            } catch (Throwable t) {
                tGL = t;
            }
        }
        if(null != tFBO) {
            throw new GLException("GLFBODrawableImpl.reset(..) FBObject.reset(..) exception", tFBO);
        }
        if(null != tGL) {
            throw new GLException("GLFBODrawableImpl.reset(..) GLContext.release() exception", tGL);
        }
        if(DEBUG) {
            System.err.println("GLFBODrawableImpl.reset(newSamples "+newSamples+"): END "+this);
        }
    }
    
    //
    // GLDrawable
    //
    
    @Override
    public final GLContext createContext(GLContext shareWith) {
        final GLContext ctx = parent.createContext(shareWith);
        ctx.setGLDrawable(this, false);
        return ctx;
    }

    //
    // GLDrawableImpl
    //
    
    @Override
    public final GLDynamicLookupHelper getGLDynamicLookupHelper() {
        return parent.getGLDynamicLookupHelper();
    }

    @Override
    protected final int getDefaultDrawFramebuffer() { return initialized ? fbos[fboIBack].getWriteFramebuffer() : 0; }
    
    @Override
    protected final int getDefaultReadFramebuffer() { return initialized ? fbos[fboIFront].getReadFramebuffer() : 0; }

    @Override
    protected final void setRealizedImpl() {
        parent.setRealized(realized);
    }
    
    @Override
    protected final void contextRealized(GLContext glc, boolean realized) {
        initialize(realized, glc.getGL());
    }
    
    @Override
    protected final void contextMadeCurrent(GLContext glc, boolean current) {
        final GL gl = glc.getGL();
        if(current) {
            fbos[fboIBack].bind(gl);
            fboBound = true;
        } else {
            if(fboBound) {
                swapFBOImpl(glc);
                fboBound=false;
                if(DEBUG) {
                    System.err.println("Post FBO swap(@release): done");
                }
            }
        }
    }
        
    @Override
    protected void swapBuffersImpl(boolean doubleBuffered) {
        final GLContext ctx = GLContext.getCurrent();
        if(null!=ctx && ctx.getGLDrawable()==this) {
            if(fboBound) {
                swapFBOImpl(ctx);
                fboBound=false;
                if(DEBUG) {
                    System.err.println("Post FBO swap(@swap): done");
                }
            }
        }
        if(null != swapBufferContext) {
            swapBufferContext.swapBuffers(doubleBuffered);
        }
    }
    
    private final void swapFBOImpl(GLContext glc) {
        final GL gl = glc.getGL();
        fbos[fboIBack].markUnbound(); // fast path, use(gl,..) is called below

        // Safely reset the previous front FBO
        if(null != pendingFBOReset) {
            reset(gl, pendingFBOReset, fboIFront, getWidth(), getHeight(), samples);
            pendingFBOReset = null;
        }
        
        if(DEBUG) {
            int _fboIFront = ( fboIFront + 1 ) % fbos.length;
            if(_fboIFront != fboIBack) { throw new InternalError("XXX: "+_fboIFront+"!="+fboIBack); }
        }
        fboIFront = fboIBack;
        fboIBack  = ( fboIBack  + 1 ) % fbos.length;
        
        final Colorbuffer colorbuffer = samples > 0 ? fbos[fboIFront].getSamplingSink() : fbos[fboIFront].getColorbuffer(0);
        final TextureAttachment texAttachment;
        if(colorbuffer instanceof TextureAttachment) {
            texAttachment = (TextureAttachment) colorbuffer;
        } else {
            if(null == colorbuffer) {
                throw new GLException("Front colorbuffer is null: samples "+samples+", "+this);
            } else {
                throw new GLException("Front colorbuffer is not a texture: "+colorbuffer.getClass().getName()+": samples "+samples+", "+colorbuffer+", "+this);
            }
        }
        gl.glActiveTexture(GL.GL_TEXTURE0 + texUnit);
        fbos[fboIFront].use(gl, texAttachment);
        
        /* Included in above use command:  
                gl.glBindFramebuffer(GL2GL3.GL_DRAW_FRAMEBUFFER, fbos[fboIBack].getDrawFramebuffer());
                gl.glBindFramebuffer(GL2GL3.GL_READ_FRAMEBUFFER, fbos[fboIFront].getReadFramebuffer());
        } */
        
        if(DEBUG) {
            System.err.println("Post FBO swap(X): fboI back "+fboIBack+", front "+fboIFront+", num "+fbos.length);
        }
    }

    //
    // GLFBODrawable
    // 
    
    @Override
    public final boolean isInitialized() {
        return initialized;
    }
    
    @Override
    public final void resetSize(GL gl) throws GLException {
        reset(gl, samples);
    }    
    
    @Override
    public final int getTextureUnit() { return texUnit; }
    
    @Override
    public final void setTextureUnit(int u) { texUnit = u; }
    
    @Override
    public final int getNumSamples() { return samples; }
    
    @Override
    public void setNumSamples(GL gl, int newSamples) throws GLException {
        if(samples != newSamples) {
            reset(gl, newSamples);
        }
    }
    
    /** // TODO: Add or remove TEXTURE (only) DoubleBufferMode support
    @Override
    public final DoubleBufferMode getDoubleBufferMode() {
        return doubleBufferMode;
    }
    
    @Override
    public final void setDoubleBufferMode(DoubleBufferMode mode) throws GLException {
        if(initialized) {
            throw new GLException("Not allowed past initialization: "+this);
        }        
        final GLCapabilitiesImmutable caps = (GLCapabilitiesImmutable) surface.getGraphicsConfiguration().getChosenCapabilities();
        if(0 == samples && caps.getDoubleBuffered() && DoubleBufferMode.NONE != mode) {
            doubleBufferMode = mode;
        }
    } */
    
    @Override
    public FBObject getFBObject(int bufferName) throws IllegalArgumentException {
        if(!initialized) {
            return null;
        }
        final FBObject res;
        switch(bufferName) {
            case GL.GL_FRONT:
                if( samples > 0 ) {
                    res = fbos[0].getSamplingSinkFBO();
                } else {
                    res = fbos[fboIFront];
                }
                break;
            case GL.GL_BACK:
                res = fbos[fboIBack];
                break;
            default: 
                throw new IllegalArgumentException(illegalBufferName+toHexString(bufferName));
        }        
        return res;  
    }
    
    @Override
    public final TextureAttachment getTextureBuffer(int bufferName) throws IllegalArgumentException {
        if(!initialized) {
            return null;
        }
        final TextureAttachment res;
        switch(bufferName) {
            case GL.GL_FRONT:
                if( samples > 0 ) {
                    res = fbos[0].getSamplingSink();
                } else {
                    res = (TextureAttachment) fbos[fboIFront].getColorbuffer(0);
                }
                break;
            case GL.GL_BACK:
                if( samples > 0 ) {
                    throw new IllegalArgumentException("Cannot access GL_BACK buffer of MSAA FBO: "+this);
                } else {
                    res = (TextureAttachment) fbos[fboIBack].getColorbuffer(0);
                }
                break;
            default: 
                throw new IllegalArgumentException(illegalBufferName+toHexString(bufferName));
        }        
        return res;  
    }
    private static final String illegalBufferName = "Only GL_FRONT and GL_BACK buffer are allowed, passed ";
    
    @Override
    public String toString() {
        return getClass().getSimpleName()+"[Initialized "+initialized+", realized "+isRealized()+", texUnit "+texUnit+", samples "+samples+
                ",\n\tFactory   "+getFactory()+
                ",\n\tHandle    "+toHexString(getHandle())+
                ",\n\tCaps      "+surface.getGraphicsConfiguration().getChosenCapabilities()+
                ",\n\tfboI back "+fboIBack+", front "+fboIFront+", num "+(initialized ? fbos.length : 0)+
                ",\n\tFBO front read "+getDefaultReadFramebuffer()+", "+getFBObject(GL.GL_FRONT)+
                ",\n\tFBO back  write "+getDefaultDrawFramebuffer()+", "+getFBObject(GL.GL_BACK)+
                ",\n\tSurface   "+getNativeSurface()+
                "]";
    }
    
    public static class ResizeableImpl extends GLFBODrawableImpl implements GLFBODrawable.Resizeable {
        protected ResizeableImpl(GLDrawableFactoryImpl factory, GLDrawableImpl parent, ProxySurface surface, 
                                 GLCapabilitiesImmutable fboCaps, int textureUnit) {
            super(factory, parent, surface, fboCaps, textureUnit);
        }
        
        @Override
        public final void setSize(GLContext context, int newWidth, int newHeight) throws NativeWindowException, GLException {
            if(DEBUG) {
                System.err.println("GLFBODrawableImpl.ResizeableImpl setSize: ("+Thread.currentThread().getName()+"): "+newWidth+"x"+newHeight+" - surfaceHandle 0x"+Long.toHexString(getNativeSurface().getSurfaceHandle()));
            }
            int lockRes = lockSurface();
            if (NativeSurface.LOCK_SURFACE_NOT_READY >= lockRes) {
                throw new NativeWindowException("Could not lock surface: "+this);
            }
            try {
                // propagate new size 
                final ProxySurface ps = (ProxySurface) getNativeSurface();
                final UpstreamSurfaceHook ush = ps.getUpstreamSurfaceHook();
                if(ush instanceof UpstreamSurfaceHook.MutableSize) {
                    ((UpstreamSurfaceHook.MutableSize)ush).setSize(newWidth, newHeight);
                } else {
                    throw new InternalError("GLFBODrawableImpl.ResizableImpl's ProxySurface doesn't hold a UpstreamSurfaceHookMutableSize but "+ush.getClass().getName()+", "+ps+", ush");
                }
                if( null != context && context.isCreated() ) {
                    resetSize(context.getGL());
                }
            } finally {
                unlockSurface();
            }
        }
    }
}
