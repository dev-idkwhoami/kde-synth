#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 origin;
    vec2 extent;
    vec2 canvasSize;
    vec2 spectrumPosition;
    vec4 colorA;
    vec4 colorB;
    float hueShift;
    float colorMode;
    float energy;
    float gradientDirection;
    float segmentHeight;
    float segmentGap;
    float padding;
    float outlineWidth;
    float glowRadius;
    float glowStrength;
    float fadeStrength;
    float shading;
    float shadeStrength;
    float flowPhase;
    float flowDirection;
    float maskEnabled;
};
layout(binding = 1) uniform sampler2D wallpaperTexture;
layout(binding = 2) uniform sampler2D shapeTexture;
vec3 rgb2hsv(vec3 c) {
    vec4 K=vec4(0.,-1./3.,2./3.,-1.);
    vec4 p=mix(vec4(c.bg,K.wz),vec4(c.gb,K.xy),step(c.b,c.g));
    vec4 q=mix(vec4(p.xyw,c.r),vec4(c.r,p.yzx),step(p.x,c.r));
    float d=q.x-min(q.w,q.y);
    return vec3(abs(q.z+(q.w-q.y)/(6.*d+1e-10)),d/(q.x+1e-10),q.x);
}
vec3 hsv2rgb(vec3 c) {
    vec3 p=abs(fract(c.xxx+vec3(0.,2./3.,1./3.))*6.-3.);
    return c.z*mix(vec3(1.),clamp(p-1.,0.,1.),c.y);
}
void main() {
    vec2 p=qt_TexCoord0*(extent+2.*padding)-padding;
    vec2 uv=clamp(p/max(extent,vec2(.001)),0.,1.);
    float horizontal=spectrumPosition.x+uv.x*spectrumPosition.y;
    float vertical=1.-uv.y;
    float flow=flowPhase+(flowDirection>.5?vertical:horizontal);
    vec3 color;
    if(colorMode<.5) {
        color=hsv2rgb(vec3(fract(horizontal*.85+energy*.10),.669, .9316));
    } else if(colorMode<1.5) {
        vec3 hsv=rgb2hsv(texture(wallpaperTexture,(origin+p)/canvasSize).rgb);
        hsv.x=fract(hsv.x+hueShift);
        hsv.z=.12+.88*sqrt(hsv.z);
        hsv.y=mix(hsv.y,hsv.y+.40*(1.-hsv.y),smoothstep(0.,.15,hsv.y));
        color=hsv2rgb(hsv);
    } else if(colorMode<2.5) {
        color=colorA.rgb;
    } else if(colorMode>4.5) {
        color=hsv2rgb(vec3(fract(flow),.669,.9316));
    } else {
        float blend=colorMode<3.5?(gradientDirection>.5?vertical:horizontal):energy;
        color=mix(colorA.rgb,colorB.rgb,clamp(blend,0.,1.));
    }
    if(shading>.5 && shading<1.5){
        float stripe=.55+.45*pow(.5+.5*cos((uv.x-.35)*6.2831853),5.);
        color=mix(color,color*stripe+vec3(.18)*pow(stripe,12.),shadeStrength);
    } else if(shading>1.5){
        float shine=.30*exp(-pow((uv.x-.28)/.20,2.))+.20*(1.-smoothstep(.15,.5,uv.y));
        color=mix(color,color*.75+vec3(shine),shadeStrength);
    }
    float radius=min(3.,min(extent.x,extent.y)*.5);
    vec2 q=abs(p-extent*.5)-extent*.5+radius;
    float d=length(max(q,0.))+min(max(q.x,q.y),0.)-radius;
    if(segmentHeight>0. && segmentGap>0.){
        float cell=mod(extent.y-p.y,segmentHeight+segmentGap);
        float segmentDistance=abs(cell-segmentHeight*.5)-segmentHeight*.5;
        d=max(d,segmentDistance);
    }
    float aa=max(fwidth(d),.001);
    float fill=1.-smoothstep(-aa,0.,d);
    if(outlineWidth>0.)fill*=smoothstep(-outlineWidth-aa,-outlineWidth,d);
    float halo=0.;
    if(glowRadius>0.){
        float haloDistance=outlineWidth>0.?abs(d+outlineWidth*.5):max(d,0.);
        halo=exp(-3.*haloDistance/glowRadius)*glowStrength*(1.-fill);
        // Fade to zero at the expanded quad boundary.
        halo*=1.-smoothstep(glowRadius*.75,glowRadius, max(d,0.));
    }
    float alpha=clamp(fill+halo,0.,1.)*(1.-fadeStrength*vertical);
    if(maskEnabled>.5){
        float shape=texture(shapeTexture,qt_TexCoord0).a;
        float glow=0.;
        if(glowRadius>0.){
            for(int i=0;i<12;++i){
                float angle=float(i)*6.2831853/12.;
                vec2 delta=vec2(cos(angle),sin(angle))*glowRadius/max(extent,vec2(1.));
                glow+=texture(shapeTexture,qt_TexCoord0+delta*.35).a*.65;
                glow+=texture(shapeTexture,qt_TexCoord0+delta*.75).a*.35;
            }
            glow=glow/12.*glowStrength;
        }
        alpha=clamp(shape+glow*(1.-shape),0.,1.)*(1.-fadeStrength*vertical);
    }
    fragColor=vec4(clamp(color,0.,1.)*alpha,alpha)*qt_Opacity;
}
