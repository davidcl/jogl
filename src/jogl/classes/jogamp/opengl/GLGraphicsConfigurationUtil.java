/**
 * Copyright 2010 JogAmp Community. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JogAmp Community ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL JogAmp Community OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of JogAmp Community.
 */

package jogamp.opengl;

import javax.media.nativewindow.AbstractGraphicsDevice;
import javax.media.opengl.GLCapabilities;
import javax.media.opengl.GLCapabilitiesImmutable;
import javax.media.opengl.GLContext;

import com.jogamp.common.os.Platform;

public class GLGraphicsConfigurationUtil {
    public static final String NV_coverage_sample = "NV_coverage_sample";
    public static final int WINDOW_BIT  = 1 << 0;
    public static final int BITMAP_BIT  = 1 << 1;
    public static final int PBUFFER_BIT = 1 << 2;
    public static final int FBO_BIT     = 1 << 3; // generic bit must be mapped to native one at impl. level
    public static final int ALL_BITS    = WINDOW_BIT | BITMAP_BIT | PBUFFER_BIT | FBO_BIT ;

    public static final StringBuilder winAttributeBits2String(StringBuilder sb, int winattrbits) {
        if(null==sb) {
            sb = new StringBuilder();
        }
        boolean seperator = false;
        if( 0 != ( WINDOW_BIT & winattrbits )  )  {
            sb.append("WINDOW");
            seperator=true;
        }
        if( 0 != ( BITMAP_BIT & winattrbits )  )  {
            if(seperator) {
                sb.append(", ");
            }
            sb.append("BITMAP");
            seperator=true;
        }
        if( 0 != ( PBUFFER_BIT & winattrbits )  )  {
            if(seperator) {
                sb.append(", ");
            }
            sb.append("PBUFFER");
            seperator=true;
        }
        if( 0 != ( FBO_BIT & winattrbits )  )  {
            if(seperator) {
                sb.append(", ");
            }
            sb.append("FBO");
        }
        return sb;
    }

    /**
    public static final int getWinAttributeBits(boolean isOnscreen, boolean isFBO, boolean isPBuffer, boolean isBitmap) {
        int winattrbits = 0;
        if(isOnscreen) {
            winattrbits |= WINDOW_BIT;
        }
        if(isFBO) {
            winattrbits |= FBO_BIT;
        } 
        if(isPBuffer ){
            winattrbits |= PBUFFER_BIT;
        } 
        if(isBitmap) {
            winattrbits |= BITMAP_BIT;                
        }
        return winattrbits;
    }
    public static final int getWinAttributeBits(GLCapabilitiesImmutable caps) {
        return getWinAttributeBits(caps.isOnscreen(), caps.isFBO(), caps.isPBuffer(), caps.isBitmap());
    } */

    /**
     * @return bitmask representing the input boolean in exclusive or logic, ie only one bit will be set.
     */
    public static final int getExclusiveWinAttributeBits(boolean isOnscreen, boolean isFBO, boolean isPBuffer, boolean isBitmap) {
        int winattrbits = 0;
        if(isOnscreen) {
            winattrbits |= WINDOW_BIT;
        } else if(isFBO) {
            winattrbits |= FBO_BIT;
        } else if(isPBuffer ){
            winattrbits |= PBUFFER_BIT;
        } else if(isBitmap) {
            winattrbits |= BITMAP_BIT;                
        }
        if(0 == winattrbits) {
            throw new InternalError("Empty bitmask");
        }
        return winattrbits;
    }

    /**
     * @see #getExclusiveWinAttributeBits(boolean, boolean, boolean, boolean)
     */
    public static final int getExclusiveWinAttributeBits(GLCapabilitiesImmutable caps) {
        return getExclusiveWinAttributeBits(caps.isOnscreen(), caps.isFBO(), caps.isPBuffer(), caps.isBitmap());
    }

    public static final GLCapabilities fixWinAttribBitsAndHwAccel(AbstractGraphicsDevice device, int winattrbits, GLCapabilities caps) {
        caps.setBitmap  ( 0 != ( BITMAP_BIT  & winattrbits ) );
        caps.setPBuffer ( 0 != ( PBUFFER_BIT & winattrbits ) );
        caps.setFBO     ( 0 != ( FBO_BIT     & winattrbits ) );
        // we reflect availability semantics, hence setting onscreen at last (maybe overwritten above)!
        caps.setOnscreen( 0 != ( WINDOW_BIT  & winattrbits ) );

        final int accel = GLContext.isHardwareRasterizer( device, caps.getGLProfile() );
        if(0 == accel && caps.getHardwareAccelerated() ) {
            caps.setHardwareAccelerated(false);
        }

        return caps;        
    }
    
    public static GLCapabilitiesImmutable fixGLCapabilities(GLCapabilitiesImmutable capsRequested, boolean fboAvailable, boolean pbufferAvailable)
    {
        if( !capsRequested.isOnscreen() ) {
            return fixOffscreenGLCapabilities(capsRequested, fboAvailable, pbufferAvailable);
        } /* we maintain the offscreen mode flags in onscreen mode - else { 
            return fixOnscreenGLCapabilities(capsRequested);
        } */
        return capsRequested;
    }

    public static GLCapabilitiesImmutable fixOnscreenGLCapabilities(GLCapabilitiesImmutable capsRequested)
    {
        if( !capsRequested.isOnscreen() || capsRequested.isFBO() || capsRequested.isPBuffer() || capsRequested.isBitmap() ) { 
            // fix caps ..
            final GLCapabilities caps2 = (GLCapabilities) capsRequested.cloneMutable();
            caps2.setBitmap  (false);
            caps2.setPBuffer (false);
            caps2.setFBO     (false);
            caps2.setOnscreen(true);
            return caps2;
        }
        return capsRequested;
    }

    public static boolean isGLCapabilitiesOffscreenAutoSelection(GLCapabilitiesImmutable capsRequested) {
        return !capsRequested.isOnscreen() &&
               !capsRequested.isFBO() && !capsRequested.isPBuffer() && !capsRequested.isBitmap() ;        
    }

    public static GLCapabilitiesImmutable fixOffscreenGLCapabilities(GLCapabilitiesImmutable capsRequested, boolean fboAvailable, boolean pbufferAvailable) {
        final boolean auto = !capsRequested.isFBO() && !capsRequested.isPBuffer() && !capsRequested.isBitmap() ;

        final boolean requestedPBuffer = capsRequested.isPBuffer() || Platform.getOSType() == Platform.OSType.MACOS ; // no native bitmap for OSX
        
        final boolean useFBO     =                fboAvailable     && ( auto || capsRequested.isFBO()     ) ;
        final boolean usePbuffer = !useFBO     && pbufferAvailable && ( auto || requestedPBuffer          ) ;
        final boolean useBitmap  = !useFBO     && !usePbuffer      && ( auto || capsRequested.isBitmap()  ) ;
        
        if( capsRequested.isOnscreen() ||
            useFBO != capsRequested.isFBO() || 
            usePbuffer != capsRequested.isPBuffer() || 
            useBitmap != capsRequested.isBitmap() ||
            useBitmap && capsRequested.getDoubleBuffered() )
        {
            // fix caps ..
            final GLCapabilities caps2 = (GLCapabilities) capsRequested.cloneMutable();
            caps2.setOnscreen(false);
            caps2.setFBO( useFBO ); 
            caps2.setPBuffer( usePbuffer );
            caps2.setBitmap( useBitmap );
            if( useBitmap ) {
                caps2.setDoubleBuffered(false);
            }
            return caps2;
        }
        return capsRequested;
    }

    public static GLCapabilitiesImmutable fixGLPBufferGLCapabilities(GLCapabilitiesImmutable capsRequested)
    {
        if( capsRequested.isOnscreen() ||
            !capsRequested.isPBuffer() || 
            capsRequested.isFBO() ) 
        {
            // fix caps ..
            final GLCapabilities caps2 = (GLCapabilities) capsRequested.cloneMutable();
            caps2.setOnscreen(false);
            caps2.setFBO(false);
            caps2.setPBuffer(true);
            caps2.setBitmap(false);
            return caps2;
        }
        return capsRequested;
    }

    /** Fix opaque setting while preserve alpha bits */
    public static GLCapabilities fixOpaqueGLCapabilities(GLCapabilities capsRequested, boolean isOpaque)
    {
        if( capsRequested.isBackgroundOpaque() != isOpaque) {
            final int alphaBits = capsRequested.getAlphaBits();
            capsRequested.setBackgroundOpaque(isOpaque);
            capsRequested.setAlphaBits(alphaBits);
        }
        return capsRequested;
    }
    
    /** Fix double buffered setting */
    public static GLCapabilitiesImmutable fixDoubleBufferedGLCapabilities(GLCapabilitiesImmutable capsRequested, boolean doubleBuffered)
    {
        if( capsRequested.getDoubleBuffered() != doubleBuffered) {
            final GLCapabilities caps2 = (GLCapabilities) capsRequested.cloneMutable();
            caps2.setDoubleBuffered(doubleBuffered);
            return caps2;
        }
        return capsRequested;
    }
}
