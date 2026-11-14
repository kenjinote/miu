//
//  SceneDelegate.m
//  miu_iOS
//
//  Created by kenji on 2026/02/23.
//

#import "SceneDelegate.h"
#import "ViewController.h"

@interface SceneDelegate ()

@end

@implementation SceneDelegate

- (ViewController *)activeEditorViewController {
    UIViewController *rootVC = self.window.rootViewController;
    if ([rootVC isKindOfClass:[UINavigationController class]]) {
        rootVC = [(UINavigationController *)rootVC topViewController];
    }
    if ([rootVC isKindOfClass:[ViewController class]]) {
        return (ViewController *)rootVC;
    }
    return nil;
}

- (void)deliverURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts {
    if (URLContexts.count == 0) return;
    UIOpenURLContext *targetContext = nil;
    for (UIOpenURLContext *ctx in URLContexts) {
        if (ctx.URL && ctx.URL.isFileURL) {
            targetContext = ctx;
            break;
        }
    }
    if (!targetContext || !targetContext.URL) return;
    NSURL *incomingURL = targetContext.URL;
    BOOL openInPlace = targetContext.options ? targetContext.options.openInPlace : YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        ViewController *vc = [self activeEditorViewController];
        if (vc) {
            [vc loadViewIfNeeded];
            [vc openIncomingURL:incomingURL openInPlace:openInPlace];
        }
    });
}


- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions {
    if (connectionOptions.URLContexts.count > 0) {
        [self deliverURLContexts:connectionOptions.URLContexts];
    }
}

- (void)scene:(UIScene *)scene openURLContexts:(NSSet<UIOpenURLContext *> *)URLContexts {
    [self deliverURLContexts:URLContexts];
}


- (void)sceneDidDisconnect:(UIScene *)scene {
    // Called as the scene is being released by the system.
    // This occurs shortly after the scene enters the background, or when its session is discarded.
    // Release any resources associated with this scene that can be re-created the next time the scene connects.
    // The scene may re-connect later, as its session was not necessarily discarded (see `application:didDiscardSceneSessions` instead).
}


- (void)sceneDidBecomeActive:(UIScene *)scene {
    // Called when the scene has moved from an inactive state to an active state.
    // Use this method to restart any tasks that were paused (or not yet started) when the scene was inactive.
}


- (void)sceneWillResignActive:(UIScene *)scene {
    // Called when the scene will move from an active state to an inactive state.
    // This may occur due to temporary interruptions (ex. an incoming phone call).
    [[self activeEditorViewController] saveRecoveryDraftIfNeeded];
}


- (void)sceneWillEnterForeground:(UIScene *)scene {
    // Called as the scene transitions from the background to the foreground.
    // Use this method to undo the changes made on entering the background.
}


- (void)sceneDidEnterBackground:(UIScene *)scene {
    // Called as the scene transitions from the foreground to the background.
    // Use this method to save data, release shared resources, and store enough scene-specific state information
    // to restore the scene back to its current state.
    [[self activeEditorViewController] saveRecoveryDraftIfNeeded];
}


@end
