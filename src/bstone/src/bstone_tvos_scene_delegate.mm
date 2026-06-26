//
// tvOS UIScene life-cycle adoption.
//
// tvOS 27 hard-traps (EXC_BREAKPOINT in
// _UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption) any app still
// using the legacy UIApplicationDelegate window life cycle. SDL2 does exactly
// that, so this minimal UIWindowSceneDelegate provides the adoption UIKit now
// requires. Declared in Info.plist via UISceneConfigurations ->
// UISceneDelegateClassName = "BStoneSceneDelegate".
//
// SDL still creates and owns the real UIWindow (on the main thread inside
// UIKit_CreateWindow). The window is bound to the active UIWindowScene from two
// sides so the ordering between scene connection and window creation doesn't
// matter:
//   - here, when the scene connects after SDL's window already exists;
//   - in the patched SDL_uikitwindow.m, when the window is created after the
//     scene already exists.
//

#import <UIKit/UIKit.h>

@interface BStoneSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation BStoneSceneDelegate

// Force the class to be registered (and never dead-stripped) so UIKit's
// NSClassFromString lookup of the Info.plist delegate name always resolves.
+ (void)load
{
}

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)connectionOptions
{
    (void)session;
    (void)connectionOptions;

    if (![scene isKindOfClass:[UIWindowScene class]]) {
        return;
    }

    // If SDL already created its window, bind it to this scene so it displays.
    id<UIApplicationDelegate> appDelegate = [UIApplication sharedApplication].delegate;
    if ([appDelegate respondsToSelector:@selector(window)]) {
        UIWindow *sdlWindow = appDelegate.window;
        if (sdlWindow != nil) {
            sdlWindow.windowScene = (UIWindowScene *)scene;
        }
    }
}

@end
