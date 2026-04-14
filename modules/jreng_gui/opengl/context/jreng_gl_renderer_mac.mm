#if JUCE_MAC

#import <Cocoa/Cocoa.h>

namespace jreng
{

void GLRenderer::enableSurfaceTransparency()
{
    NSOpenGLContext* ctx = [NSOpenGLContext currentContext];

    if (ctx != nil)
    {
        GLint opaque = 0;
        [ctx setValues:&opaque forParameter:NSOpenGLContextParameterSurfaceOpacity];
    }
}

} // namespace jreng

#endif
