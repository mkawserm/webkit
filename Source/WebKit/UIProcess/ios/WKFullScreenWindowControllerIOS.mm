/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if PLATFORM(IOS) && ENABLE(FULLSCREEN_API)
#import "WKFullScreenWindowControllerIOS.h"

#import "UIKitSPI.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIVisualEffectView.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/IntRect.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

using namespace WebKit;
using namespace WebCore;

namespace WebKit {

static void replaceViewWithView(UIView *view, UIView *otherView)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [otherView setFrame:[view frame]];
    [otherView setAutoresizingMask:[view autoresizingMask]];
    [[view superview] insertSubview:otherView aboveSubview:view];
    [view removeFromSuperview];
    [CATransaction commit];
}

enum FullScreenState : NSInteger {
    NotInFullScreen,
    WaitingToEnterFullScreen,
    EnteringFullScreen,
    InFullScreen,
    WaitingToExitFullScreen,
    ExitingFullScreen,
};

struct WKWebViewState {
    float _savedTopContentInset = 0.0;
    CGFloat _savedPageScale = 1;
    CGFloat _savedViewScale = 1.0;
    UIEdgeInsets _savedEdgeInset = UIEdgeInsetsZero;
    UIEdgeInsets _savedObscuredInsets = UIEdgeInsetsZero;
    UIEdgeInsets _savedScrollIndicatorInsets = UIEdgeInsetsZero;
    CGPoint _savedContentOffset = CGPointZero;
    
    void applyTo(WKWebView* webView)
    {
        [webView _setPageScale:_savedPageScale withOrigin:CGPointMake(0, 0)];
        [webView _setObscuredInsets:_savedObscuredInsets];
        [[webView scrollView] setContentInset:_savedEdgeInset];
        [[webView scrollView] setContentOffset:_savedContentOffset];
        [[webView scrollView] setScrollIndicatorInsets:_savedScrollIndicatorInsets];
        [webView _page]->setTopContentInset(_savedTopContentInset);
        [webView _setViewScale:_savedViewScale];
    }
    
    void store(WKWebView* webView)
    {
        _savedPageScale = [webView _pageScale];
        _savedObscuredInsets = [webView _obscuredInsets];
        _savedEdgeInset = [[webView scrollView] contentInset];
        _savedContentOffset = [[webView scrollView] contentOffset];
        _savedScrollIndicatorInsets = [[webView scrollView] scrollIndicatorInsets];
        _savedTopContentInset = [webView _page]->topContentInset();
        _savedViewScale = [webView _viewScale];
    }
};
    
} // namespace WebKit


@interface _WKFullScreenViewController : UIViewController <UIGestureRecognizerDelegate>
@property (retain, nonatomic) NSArray *savedConstraints;
@property (retain, nonatomic) UIView *contentView;
@property (retain, nonatomic) id target;
@property (assign, nonatomic) SEL action;
@end

@implementation _WKFullScreenViewController {
    RetainPtr<UIView> _backgroundView;
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<UIButton> _cancelButton;
    RetainPtr<UIVisualEffectView> _visualEffectView;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [super dealloc];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [(WKWebView *)[self contentView] _overrideLayoutParametersWithMinimumLayoutSize:size maximumUnobscuredSizeOverride:size];
}

+ (void)configureView:(UIView *)view withBackgroundFillOfColor:(UIColor *)fillColor opacity:(CGFloat)opacity filter:(NSString *)filter
{
    _UIVisualEffectLayerConfig *baseLayerConfig = [_UIVisualEffectLayerConfig layerWithFillColor:fillColor opacity:opacity filterType:filter];
    [[[_UIVisualEffectConfig configWithContentConfig:baseLayerConfig] contentConfig] configureLayerView:view];
}

- (void)_updateTransparencyOfVisualEffectView:(UIVisualEffectView *)visualEffectView
{
    RetainPtr<UIVisualEffect> visualEffect;

    if (UIAccessibilityIsReduceTransparencyEnabled()) {
        visualEffect = [UIVisualEffect emptyEffect];
        [[visualEffectView contentView] setBackgroundColor:[UIColor colorWithRed:(43.0 / 255.0) green:(46.0 / 255.0) blue:(48.0 / 255.0) alpha:1.0]];
    } else {
        RetainPtr<UIColorEffect> saturationEffect = [UIColorEffect colorEffectSaturate:1.8];
        RetainPtr<UIBlurEffect> blurEffect = [UIBlurEffect effectWithBlurRadius:UIRoundToScreenScale(17.5, [UIScreen mainScreen])];
        RetainPtr<UIVisualEffect> combinedEffects = [UIVisualEffect effectCombiningEffects:@[blurEffect.get(), saturationEffect.get()]];
        visualEffect = combinedEffects;
        [[visualEffectView contentView] setBackgroundColor:nil];
    }

    [visualEffectView setEffect:visualEffect.get()];
}

- (UIVisualEffectView *)visualEffectViewWithFrame:(CGRect)frame
{
    RetainPtr<UIVisualEffectView> visualEffectView = adoptNS([[UIVisualEffectView alloc] initWithEffect:[UIVisualEffect emptyEffect]]);
    [visualEffectView setFrame:frame];
    [visualEffectView _setContinuousCornerRadius:((CGRectGetHeight([visualEffectView bounds]) > 40.0) ? 16.0 : 8.0)];
    [visualEffectView setAutoresizingMask:(UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
    [self _updateTransparencyOfVisualEffectView:visualEffectView.get()];

    RetainPtr<UIView> backLayerTintView = adoptNS([[UIView alloc] initWithFrame:[visualEffectView bounds]]);
    [backLayerTintView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [backLayerTintView setUserInteractionEnabled:NO];
    [[self class] configureView:backLayerTintView.get() withBackgroundFillOfColor:[UIColor colorWithWhite:0.0 alpha:0.55] opacity:1.0 filter:kCAFilterNormalBlendMode];
    [[visualEffectView contentView] addSubview:backLayerTintView.get()];

    RetainPtr<UIView> topLayerTintView = adoptNS([[UIView alloc] initWithFrame:[visualEffectView bounds]]);
    [topLayerTintView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [topLayerTintView setUserInteractionEnabled:NO];
    [[self class] configureView:topLayerTintView.get() withBackgroundFillOfColor:[UIColor colorWithWhite:1.0 alpha:0.14] opacity:1.0 filter:kCAFilterNormalBlendMode];
    [[visualEffectView contentView] addSubview:topLayerTintView.get()];

    [self _updateTransparencyOfVisualEffectView:visualEffectView.get()];

    return visualEffectView.autorelease();
}

- (void)loadView
{
    [self setView:adoptNS([[UIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]]).get()];
    [[self view] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];

    CGRect doneButtonRect = CGRectMake(10, 20, 60, 47);
    
    _visualEffectView = [self visualEffectViewWithFrame:doneButtonRect];
    [_visualEffectView setAlpha:0];
    [[self view] addSubview:_visualEffectView.get()];

    _cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setAutoresizingMask:(UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin)];
    [_cancelButton setFrame:doneButtonRect];
    [_cancelButton addTarget:self action:@selector(cancelAction:) forControlEvents:UIControlEventTouchUpInside];
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
    [_cancelButton setBackgroundColor: [UIColor blackColor]];
    [[_cancelButton layer] setCornerRadius:6];
    [_cancelButton setAlpha:0];
    NSBundle *bundle = [NSBundle bundleForClass:[WKFullScreenWindowController class]];
    RetainPtr<UIImage> image = [UIImage imageNamed:@"Done" inBundle:bundle compatibleWithTraitCollection:nil];
    [_cancelButton setImage:[image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];

    [_cancelButton setTintColor:[UIColor colorWithWhite:1.0 alpha:0.55]];
    [[_cancelButton layer] setCompositingFilter:[CAFilter filterWithType:kCAFilterPlusL]];

    [[self view] addSubview:_cancelButton.get()];

    _touchGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(showCancelButton:)]);
    [_touchGestureRecognizer setDelegate:self];
    [_touchGestureRecognizer setCancelsTouchesInView:NO];
    [_touchGestureRecognizer setMinimumPressDuration:0];
    [[self view] addGestureRecognizer:_touchGestureRecognizer.get()];
}

- (void)viewWillAppear:(BOOL)animated
{
    [[self contentView] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [[self contentView] setFrame:[[self view] bounds]];
    [[self view] insertSubview:[self contentView] atIndex:0];
}

- (void)viewDidDisappear:(BOOL)animated
{
}

- (void)cancelAction:(id)sender
{
    [[self target] performSelector:[self action]];
}

- (void)hideCancelButton
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideCancelButton) object:nil];
    [UIView animateWithDuration:0.2 animations:^{
        [_visualEffectView setAlpha:0];
        [_cancelButton setAlpha:0];
    } completion:^(BOOL finished){
        if (finished) {
            [_cancelButton setHidden:YES];
            [_visualEffectView setHidden:YES];
        }
    }];
}

- (void)showCancelButton:(id)sender
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideCancelButton) object:nil];
    [self performSelector:@selector(hideCancelButton) withObject:nil afterDelay:3.0];
    [UIView animateWithDuration:0.2 animations: ^{
        [_visualEffectView setHidden:NO];
        [_cancelButton setHidden:NO];
        [_visualEffectView setAlpha:1];
        [_cancelButton setAlpha:1];
    }];
}

- (void)setTarget:(id)target action:(SEL)action
{
    [self setTarget:target];
    [self setAction:action];
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

// MARK - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

@end


@interface WKFullscreenAnimationController : NSObject <UIViewControllerAnimatedTransitioning>
@property (retain, nonatomic) UIViewController* viewController;
@property (nonatomic) CGRect initialFrame;
@property (nonatomic) CGRect finalFrame;
@property (nonatomic, getter=isAnimatingIn) BOOL animatingIn;
@end

@implementation WKFullscreenAnimationController

- (NSTimeInterval)transitionDuration:(id<UIViewControllerContextTransitioning>)transitionContext
{
    const NSTimeInterval animationDuration = 0.2;
    return animationDuration;
}

- (void)animateTransition:(id<UIViewControllerContextTransitioning>)transitionContext
{
    UIView *containerView = [transitionContext containerView];
    UIView *fromView = [transitionContext viewForKey:UITransitionContextFromViewKey];
    UIView *toView = [transitionContext viewForKey:UITransitionContextToViewKey];

    CGRect inlineFrame = _animatingIn ? _initialFrame : _finalFrame;
    CGRect fullscreenFrame = _animatingIn ? _finalFrame : _initialFrame;
    UIView *animatingView = _animatingIn ? toView : fromView;
    
    CGRect boundsRect = largestRectWithAspectRatioInsideRect(FloatRect(inlineFrame).size().aspectRatio(), fullscreenFrame);
    boundsRect.origin = CGPointZero;
    RetainPtr<UIView> maskView = adoptNS([[UIView alloc] init]);
    [maskView setBackgroundColor:[UIColor blackColor]];
    [maskView setBounds:_animatingIn ? boundsRect : [animatingView bounds]];
    [maskView setCenter:CGPointMake(CGRectGetMidX([animatingView bounds]), CGRectGetMidY([animatingView bounds]))];
    [animatingView setMaskView:maskView.get()];
    
    FloatRect scaleRect = smallestRectWithAspectRatioAroundRect(FloatRect(fullscreenFrame).size().aspectRatio(), inlineFrame);
    CGAffineTransform scaleTransform = CGAffineTransformMakeScale(scaleRect.width() / fullscreenFrame.size.width, scaleRect.height() / fullscreenFrame.size.height);
    CGAffineTransform translateTransform = CGAffineTransformMakeTranslation(CGRectGetMidX(inlineFrame) - CGRectGetMidX(fullscreenFrame), CGRectGetMidY(inlineFrame) - CGRectGetMidY(fullscreenFrame));
    
    CGAffineTransform finalTransform = CGAffineTransformConcat(scaleTransform, translateTransform);

    [animatingView setTransform:_animatingIn ? finalTransform : CGAffineTransformIdentity];
    
    [containerView addSubview:animatingView];

    [UIView animateWithDuration:[self transitionDuration:transitionContext] delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        [animatingView setTransform:_animatingIn ? CGAffineTransformIdentity : finalTransform];
        [maskView setBounds:_animatingIn ? animatingView.bounds : boundsRect];
        [maskView setCenter:CGPointMake(CGRectGetMidX([animatingView bounds]), CGRectGetMidY([animatingView bounds]))];
    } completion:^(BOOL finished){
        BOOL success = ![transitionContext transitionWasCancelled];

        if (([self isAnimatingIn] && !success) || (![self isAnimatingIn] && success))
            [animatingView removeFromSuperview];

        [transitionContext completeTransition:success];
        [animatingView setMaskView:nil];
    }];
}

- (void)animationEnded:(BOOL)transitionCompleted
{
}

@end

@implementation WKFullScreenWindowController {
    WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
    RetainPtr<UIView> _webViewPlaceholder;

    FullScreenState _fullScreenState;
    WKWebViewState _viewState;

    RefPtr<WebKit::VoidCallback> _repaintCallback;
    RetainPtr<UIViewController> _viewControllerForPresentation;
    RetainPtr<_WKFullScreenViewController> _fullscreenViewController;

    CGRect _initialFrame;
    CGRect _finalFrame;
}

#pragma mark -
#pragma mark Initialization
- (id)initWithWebView:(WKWebView *)webView
{
    if (![super init])
        return nil;
    _webView = webView;

    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [super dealloc];
}

#pragma mark -
#pragma mark Accessors

- (BOOL)isFullScreen
{
    return _fullScreenState == WaitingToEnterFullScreen
        || _fullScreenState == EnteringFullScreen
        || _fullScreenState == InFullScreen;
}

- (WebCoreFullScreenPlaceholderView *)webViewPlaceholder
{
    return nil;
}

#pragma mark -
#pragma mark Exposed Interface

- (void)enterFullScreen
{
    if ([self isFullScreen])
        return;

    _fullScreenState = WaitingToEnterFullScreen;
    
    _viewControllerForPresentation = [UIViewController _viewControllerForFullScreenPresentationFromView:_webView];
    _fullscreenViewController = adoptNS([[_WKFullScreenViewController alloc] init]);
    [_fullscreenViewController setTransitioningDelegate:self];
    [_fullscreenViewController setModalPresentationStyle:UIModalPresentationCustom];
    [_fullscreenViewController setTarget:self action:@selector(requestExitFullScreen)];
    
    [self _manager]->saveScrollPosition();

    [_webView _page]->setSuppressVisibilityUpdates(true);

    _viewState.store(_webView);

    _webViewPlaceholder = adoptNS([[UIView alloc] init]);
    [[_webViewPlaceholder layer] setName:@"Fullscreen Placeholder Vfiew"];

    WKSnapshotConfiguration* config = nil;
    [_webView takeSnapshotWithConfiguration:config completionHandler:^(UIImage * snapshotImage, NSError * error){
        UIScreen* screen = [UIScreen mainScreen];
        RetainPtr<UIWindow> webWindow = [_webView window];
        
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        
        [[_webViewPlaceholder layer] setContents:(id)[snapshotImage CGImage]];
        replaceViewWithView(_webView, _webViewPlaceholder.get());
        
        WKWebViewState().applyTo(_webView);
        
        [_webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        [_webView setFrame:[screen bounds]];
        [webWindow insertSubview:_webView atIndex:0];
        [_webView _overrideLayoutParametersWithMinimumLayoutSize:[screen bounds].size maximumUnobscuredSizeOverride:[screen bounds].size];
        
        [_webView setNeedsLayout];
        [_webView layoutIfNeeded];
        
        [self _manager]->setAnimatingFullScreen(true);
        
        // FIXME: <http://webkit.org/b/178923> Find a better way to do this.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0), dispatch_get_main_queue(), ^{
            _repaintCallback = VoidCallback::create([protectedSelf = RetainPtr<WKFullScreenWindowController>(self)](WebKit::CallbackBase::Error) {
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0), dispatch_get_main_queue(), ^{
                    [protectedSelf _manager]->willEnterFullScreen();
                });
            });
            [_webView _page]->forceRepaint(_repaintCallback.copyRef());
        });
        
        [CATransaction commit];
    }];
}

- (void)beganEnterFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WaitingToEnterFullScreen)
        return;
    _fullScreenState = EnteringFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;
    
    [[_fullscreenViewController view] setFrame:[[UIScreen mainScreen] bounds]];
    [_fullscreenViewController setContentView:_webView];
    [_viewControllerForPresentation presentViewController:_fullscreenViewController.get() animated:YES completion:^{
        [self completedEnterFullScreen];
    }];
}

- (void)completedEnterFullScreen
{
    _fullScreenState = InFullScreen;
    [self _manager]->didEnterFullScreen();
    [self _manager]->setAnimatingFullScreen(false);

    [_webView _page]->setSuppressVisibilityUpdates(false);
}

- (void)exitFullScreen
{
    if (![self isFullScreen])
        return;
    _fullScreenState = WaitingToExitFullScreen;

    [self _manager]->setAnimatingFullScreen(true);
    [self _manager]->willExitFullScreen();
}

- (void)requestExitFullScreen
{
    [self _manager]->requestExitFullScreen();
}

- (void)beganExitFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WaitingToExitFullScreen)
        return;
    _fullScreenState = ExitingFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;
    
    [_webView _page]->setSuppressVisibilityUpdates(true);

    [_fullscreenViewController dismissViewControllerAnimated:YES completion:^{
        [self completedExitFullScreen];
    }];
}

- (void)completedExitFullScreen
{
    _fullScreenState = NotInFullScreen;

    [_webView setFrame:[_webViewPlaceholder bounds]];
    [[_webViewPlaceholder window] insertSubview:_webView atIndex:0];

    [[_webView window] makeKeyAndVisible];

    [self _manager]->didExitFullScreen();
    [self _manager]->setAnimatingFullScreen(false);

    _viewState.applyTo(_webView);

    [_webView setNeedsLayout];
    [_webView layoutIfNeeded];

    if (_repaintCallback) {
        _repaintCallback->invalidate(WebKit::CallbackBase::Error::OwnerWasInvalidated);
        ASSERT(!_repaintCallback);
    }
    _repaintCallback = VoidCallback::create([protectedSelf = RetainPtr<WKFullScreenWindowController>(self), self](WebKit::CallbackBase::Error) {
        replaceViewWithView(_webViewPlaceholder.get(), _webView);
        _repaintCallback = nullptr;
        [_webView _page]->setSuppressVisibilityUpdates(false);
    });
    [_webView _page]->forceRepaint(_repaintCallback.copyRef());
}

- (void)close
{
    if ([self isFullScreen])
        [self exitFullScreen];
    
    _webView = nil;
}

#pragma mark -
#pragma mark Internal Interface

- (WebFullScreenManagerProxy*)_manager
{
    if (![_webView _page])
        return nullptr;
    return [_webView _page]->fullScreenManager();
}

#pragma mark -
#pragma mark UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)animationControllerForPresentedController:(UIViewController *)presented presentingController:(UIViewController *)presenting sourceController:(UIViewController *)source
{
    RetainPtr<WKFullscreenAnimationController> animationController = adoptNS([[WKFullscreenAnimationController alloc] init]);
    [animationController setViewController:presented];
    [animationController setInitialFrame:_initialFrame];
    [animationController setFinalFrame:_finalFrame];
    [animationController setAnimatingIn:YES];
    return animationController.autorelease();
}

- (id<UIViewControllerAnimatedTransitioning>)animationControllerForDismissedController:(UIViewController *)dismissed
{
    RetainPtr<WKFullscreenAnimationController> animationController = adoptNS([[WKFullscreenAnimationController alloc] init]);
    [animationController setViewController:dismissed];
    [animationController setInitialFrame:_initialFrame];
    [animationController setFinalFrame:_finalFrame];
    [animationController setAnimatingIn:NO];
    return animationController.autorelease();
}

@end

#endif // PLATFORM(IOS) && ENABLE(FULLSCREEN_API)
