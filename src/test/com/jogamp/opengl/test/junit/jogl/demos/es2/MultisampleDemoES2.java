/*
 * Copyright (c) 2003 Sun Microsystems, Inc. All Rights Reserved.
 * Copyright (c) 2010 JogAmp Community. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistribution of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistribution in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN
 * MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE FOR
 * ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
 * DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR
 * ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR
 * DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE
 * DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY,
 * ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
 * SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 *
 * Sun gratefully acknowledges that this software was originally authored
 * and developed by Kenneth Bradley Russell and Christopher John Kline.
 */

package com.jogamp.opengl.test.junit.jogl.demos.es2;

import javax.media.opengl.GL;
import javax.media.opengl.GL2ES2;
import javax.media.opengl.GLAutoDrawable;
import javax.media.opengl.GLEventListener;
import javax.media.opengl.GLUniformData;
import javax.media.opengl.fixedfunc.GLMatrixFunc;

import com.jogamp.opengl.util.ImmModeSink;
import com.jogamp.opengl.util.PMVMatrix;
import com.jogamp.opengl.util.glsl.ShaderCode;
import com.jogamp.opengl.util.glsl.ShaderProgram;
import com.jogamp.opengl.util.glsl.ShaderState;

public class MultisampleDemoES2 implements GLEventListener {

    private boolean multisample;
    private final ShaderState st;
    private final PMVMatrix pmvMatrix;
    private ShaderProgram sp0;
    private GLUniformData pmvMatrixUniform;
    private ImmModeSink immModeSink;

    public MultisampleDemoES2(boolean multisample) {
        this.multisample = multisample;
        st = new ShaderState();
        st.setVerbose(true);        
        pmvMatrix = new PMVMatrix();        
    }

    static final String[] es2_prelude = { "#version 100\n", "precision mediump float;\n" };
    static final String gl2_prelude = "#version 110\n";
    
    public void init(GLAutoDrawable glad) {
        final GL2ES2 gl = glad.getGL().getGL2ES2();
        System.err.println();
        System.err.println("Requested: " + glad.getNativeSurface().getGraphicsConfiguration().getRequestedCapabilities());
        System.err.println();
        System.err.println("Chosen   : " + glad.getChosenGLCapabilities());
        System.err.println();
        
        final ShaderCode vp0 = ShaderCode.create(gl, GL2ES2.GL_VERTEX_SHADER, MultisampleDemoES2.class, "shader",
                "shader/bin", "mgl_default_xxx", true);
        final ShaderCode fp0 = ShaderCode.create(gl, GL2ES2.GL_FRAGMENT_SHADER, MultisampleDemoES2.class, "shader",
                "shader/bin", "mgl_default_xxx", true);
        
        // Prelude shader code w/ GLSL profile specifics [ 1. pre-proc, 2. other ]
        int fp0Pos;
        if(gl.isGLES2()) {
            vp0.insertShaderSource(0, 0, es2_prelude[0]);
            fp0Pos = fp0.insertShaderSource(0, 0, es2_prelude[0]);
        } else {
            vp0.insertShaderSource(0, 0, gl2_prelude);
            fp0Pos = fp0.insertShaderSource(0, 0, gl2_prelude);
        }
        if(gl.isGLES2()) {
            fp0Pos = fp0.insertShaderSource(0, fp0Pos, es2_prelude[1]);
        }        
        
        sp0 = new ShaderProgram();
        sp0.add(gl, vp0, System.err);
        sp0.add(gl, fp0, System.err);       
        st.attachShaderProgram(gl, sp0, true);
        
        pmvMatrixUniform = new GLUniformData("mgl_PMVMatrix", 4, 4, pmvMatrix.glGetPMvMatrixf());
        st.ownUniform(pmvMatrixUniform);       
        st.uniform(gl, pmvMatrixUniform);
        
        // Using predef array names, see 
        //    GLPointerFuncUtil.getPredefinedArrayIndexName(glArrayIndex);
        immModeSink = ImmModeSink.createGLSL(gl, GL.GL_STATIC_DRAW, 40, 
                                              3, GL.GL_FLOAT,  // vertex
                                              4, GL.GL_FLOAT,  // color
                                              0, GL.GL_FLOAT,// normal
                                              0, GL.GL_FLOAT); // texture
        final int numSteps = 20;
        final double increment = Math.PI / numSteps;
        final double radius = 1;
        immModeSink.glBegin(GL.GL_LINES);
        for (int i = numSteps - 1; i >= 0; i--) {
            immModeSink.glVertex3f((float) (radius * Math.cos(i * increment)), 
                                   (float) (radius * Math.sin(i * increment)), 
                                   0f);
            immModeSink.glColor4f( 1f, 1f, 1f, 1f ); 
            immModeSink.glVertex3f((float) (-1.0 * radius * Math.cos(i * increment)), 
                                   (float) (-1.0 * radius * Math.sin(i * increment)), 
                                   0f);
            immModeSink.glColor4f( 1f, 1f, 1f, 1f ); 
        }
        immModeSink.glEnd(gl, false);
        
        st.useProgram(gl, false);
    }

    public void dispose(GLAutoDrawable glad) {
        final GL2ES2 gl = glad.getGL().getGL2ES2();
        immModeSink.destroy(gl);
        immModeSink = null;
        st.destroy(gl);
    }

    public void display(GLAutoDrawable glad) {
        final GL2ES2 gl = glad.getGL().getGL2ES2();
        if (multisample) {
            gl.glEnable(GL.GL_MULTISAMPLE);
        }
        gl.glClearColor(0, 0, 0, 0);
        //      gl.glEnable(GL.GL_DEPTH_TEST);
        //      gl.glDepthFunc(GL.GL_LESS);
        gl.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT);
        
        st.useProgram(gl, true);
        
        immModeSink.draw(gl, true);
        
        st.useProgram(gl, false);
    }

    // Unused routines
    public void reshape(GLAutoDrawable glad, int x, int y, int width, int height) {
        System.err.println("reshape ..");
        final GL2ES2 gl = glad.getGL().getGL2ES2();
        pmvMatrix.glMatrixMode(GLMatrixFunc.GL_PROJECTION);
        pmvMatrix.glLoadIdentity();
        // pmvMatrix.glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        pmvMatrix.glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 10.0f);
        pmvMatrix.glMatrixMode(GLMatrixFunc.GL_MODELVIEW);
        pmvMatrix.glLoadIdentity();
        
        st.useProgram(gl, true);
        st.uniform(gl, pmvMatrixUniform);
        st.useProgram(gl, false);
    }

    public void displayChanged(GLAutoDrawable drawable, boolean modeChanged, boolean deviceChanged) {
    }
}
