/* Приложение для iPhone и iPad: игра рисует кадр в свой буфер (тот же код, что под Windows, через
 * win32_shim.h и plat_gdi.c), здесь он показывается на экране, а касания превращаются в «мышь»:
 *   касание — щелчок; долгое нажатие — правая кнопка (приказ армии);
 *   палец по карте — сдвиг карты; палец по спискам — прокрутка;
 *   два пальца на карте — щипок приближает, поворот и движение вверх-вниз вращают и наклоняют карту.
 * ui.h сюда не подключается: в нём BOOL из Win32, а у Objective-C свой. */
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include "plat_host.h"
#include <math.h>
#include <locale.h>

unsigned *d_bits(void);

static int s_dirty = 1;
typedef struct { int on, ms; CFTimeInterval next; } HostTimer;
static HostTimer s_tm[8];
static char s_dataDir[1024], s_resDir[1024];

void host_invalidate(void) { s_dirty = 1; }
void host_set_timer(int id, int ms)
{
    if (id < 0 || id >= 8) return;
    s_tm[id].on = 1;
    s_tm[id].ms = ms < 1 ? 1 : ms;
    s_tm[id].next = CACurrentMediaTime() + s_tm[id].ms / 1000.0;
}
void host_kill_timer(int id) { if (id >= 0 && id < 8) s_tm[id].on = 0; }
const char *host_data_dir(void) { return s_dataDir; }
const char *host_res_dir(void) { return s_resDir; }
void host_quit(void) { s_dirty = 1; }     /* приложения iPhone сами не закрываются; партия уже сохранена */

enum { T_WAIT, T_PAN, T_SCROLL, T_TWO, T_DONE };

@interface GameView : UIView
@end

@implementation GameView {
    CADisplayLink *_link;
    int _w, _h, _started;
    CGFloat _k;                    /* пикселей буфера на точку экрана */
    UITouch *_t1, *_t2;
    CGPoint _p0, _plast;           /* один палец: начало и последнее положение (пиксели) */
    CFTimeInterval _t0;
    int _mode;
    double _scrollAcc;
    CGPoint _c0;                   /* два пальца: середина, расстояние, угол */
    double _d0, _a0, _vx, _vy, _zoomAcc;
    int _twoMap, _rotating;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if ((self = [super initWithFrame:frame])) {
        self.multipleTouchEnabled = YES;
        self.backgroundColor = UIColor.blackColor;
        self.layer.contentsGravity = kCAGravityResize;
    }
    return self;
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];
    if (self.window && !_link) {
        _link = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
        [_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    }
}

- (CGPoint)px:(UITouch *)t
{
    CGPoint p = [t locationInView:self];
    return CGPointMake(p.x * _k, p.y * _k);
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    _k = self.window.screen.scale > 0 ? self.window.screen.scale : UIScreen.mainScreen.scale;
    int w = (int)lround(self.bounds.size.width * _k), h = (int)lround(self.bounds.size.height * _k);
    if (w < 64 || h < 64) return;
    if (!_started) {
        /* масштаб интерфейса: экран телефона — как окно примерно 1100 x 560 */
        double dpi = fmin(h / 560.0, w / 1100.0);
        if (dpi < 1) dpi = 1;
        wchar_t cmd[64] = { 0 };
        for (NSString *a in NSProcessInfo.processInfo.arguments)
            if ([a hasPrefix:@"--scene="]) swprintf(cmd, 64, L"--scene=%d", [[a substringFromIndex:8] intValue]);
        ui_ev_start((float)dpi, cmd[0] ? cmd : NULL);
        host_set_timer(2, 1000);       /* часы минутного дохода */
        _started = 1;
    }
    if (w != _w || h != _h) {
        _w = w; _h = h;
        ui_ev_size(w, h);
    }
    s_dirty = 1;
}

- (void)present
{
    const unsigned *px = d_bits();
    if (!px || _w <= 0 || _h <= 0) return;
    CFDataRef data = CFDataCreate(NULL, (const UInt8 *)px, (CFIndex)_w * _h * 4);
    CGDataProviderRef prov = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    /* пиксель буфера — 0x00RRGGBB в порядке байтов процессора: B, G, R, пусто */
    CGImageRef img = CGImageCreate((size_t)_w, (size_t)_h, 8, 32, (size_t)_w * 4, cs,
                                   kCGBitmapByteOrder32Little | (CGBitmapInfo)kCGImageAlphaNoneSkipFirst, prov, NULL, false, kCGRenderingIntentDefault);
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.layer.contents = (__bridge id)img;
    [CATransaction commit];
    CGImageRelease(img);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    CFRelease(data);
}

- (void)tick:(CADisplayLink *)link
{
    if (!_started) return;
    CFTimeInterval now = CACurrentMediaTime();
    for (int i = 0; i < 8; i++)
        if (s_tm[i].on && now >= s_tm[i].next) {
            s_tm[i].next = now + s_tm[i].ms / 1000.0;
            ui_ev_timer(i);
        }
    /* долгое нажатие — правая кнопка мыши */
    if (_t1 && !_t2 && _mode == T_WAIT && now - _t0 > 0.55) {
        _mode = T_DONE;
        [[[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium] impactOccurred];
        ui_ev_move((int)_p0.x, (int)_p0.y);
        ui_ev_down((int)_p0.x, (int)_p0.y, 1);
        ui_ev_up((int)_p0.x, (int)_p0.y, 1);
    }
    if (s_dirty) {
        s_dirty = 0;
        if (ui_ev_paint()) host_set_timer(4, 33);
        [self present];
    }
}

/* ---------- касания ---------- */
- (void)beginTwo
{
    CGPoint a = [self px:_t1], b = [self px:_t2];
    if (_mode == T_PAN) ui_ev_up((int)_plast.x, (int)_plast.y, 1);
    _mode = T_TWO;
    _c0 = CGPointMake((a.x + b.x) / 2, (a.y + b.y) / 2);
    _d0 = hypot(b.x - a.x, b.y - a.y);
    _a0 = atan2(b.y - a.y, b.x - a.x);
    _vx = _c0.x; _vy = _c0.y;
    _zoomAcc = 0;
    _rotating = 0;
    _twoMap = ui_ev_on_map((int)_c0.x, (int)_c0.y);
}

- (void)moveTwo
{
    CGPoint a = [self px:_t1], b = [self px:_t2], c = CGPointMake((a.x + b.x) / 2, (a.y + b.y) / 2);
    double d = hypot(b.x - a.x, b.y - a.y), ang = atan2(b.y - a.y, b.x - a.x);
    if (!_twoMap) return;
    if (_d0 > 1 && d > 1) {                 /* щипок: колесо мыши (карта приближается в pow(1.18, delta/120) раз) */
        _zoomAcc += 120.0 * log(d / _d0) / log(1.18);
        int z = (int)_zoomAcc;
        if (z) { ui_ev_wheel(z, (int)c.x, (int)c.y, 0); _zoomAcc -= z; }
    }
    double da = ang - _a0;
    while (da > M_PI) da -= 2 * M_PI;
    while (da < -M_PI) da += 2 * M_PI;
    double nx = _vx + da * 180 / M_PI / 0.35, ny = _vy + (c.y - _c0.y);     /* поворот и наклон — как левая кнопка */
    if (!_rotating && (fabs(nx - _c0.x) > 8 || fabs(ny - _c0.y) > 8)) {
        _rotating = 1;
        ui_ev_down((int)_vx, (int)_vy, 0);
    }
    _vx = nx; _vy = ny;
    if (_rotating) ui_ev_move((int)_vx, (int)_vy);
    _d0 = d; _a0 = ang; _c0 = c;
}

- (void)endTwo
{
    if (_twoMap && _rotating) ui_ev_up((int)_vx, (int)_vy, 0);
    _rotating = 0;
    _mode = T_DONE;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *t in touches) {
        if (!_t1) {
            _t1 = t;
            _p0 = _plast = [self px:t];
            _t0 = CACurrentMediaTime();
            _mode = T_WAIT;
            _scrollAcc = 0;
            ui_ev_move((int)_p0.x, (int)_p0.y);
        } else if (!_t2 && _mode != T_DONE) {
            _t2 = t;
            [self beginTwo];
        }
    }
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (_mode == T_TWO && _t1 && _t2) { [self moveTwo]; return; }
    if (!_t1 || ![touches containsObject:_t1]) return;
    CGPoint p = [self px:_t1];
    if (_mode == T_WAIT && hypot(p.x - _p0.x, p.y - _p0.y) > 10 * _k) {
        if (ui_ev_on_map((int)_p0.x, (int)_p0.y)) {
            _mode = T_PAN;
            ui_ev_down((int)_p0.x, (int)_p0.y, 1);       /* сдвиг карты — как правая кнопка */
        } else _mode = T_SCROLL;
    }
    if (_mode == T_PAN) ui_ev_move((int)p.x, (int)p.y);
    else if (_mode == T_SCROLL) {
        _scrollAcc += (p.y - _plast.y) * 120.0 / ui_ev_scroll_unit();
        int d = (int)_scrollAcc;
        if (d) { ui_ev_wheel(d, (int)_p0.x, (int)_p0.y, 0); _scrollAcc -= d; }
    }
    _plast = p;
}

- (void)finish:(NSSet<UITouch *> *)touches cancelled:(BOOL)cancelled
{
    for (UITouch *t in touches) {
        if (t != _t1 && t != _t2) continue;
        if (_mode == T_TWO) [self endTwo];
        else if (t == _t1) {
            if (_mode == T_WAIT && !cancelled) {             /* касание — щелчок левой кнопкой */
                int x = (int)_p0.x, y = (int)_p0.y;
                ui_ev_move(x, y);
                ui_ev_down(x, y, 0);
                ui_ev_up(x, y, 0);
            } else if (_mode == T_PAN) ui_ev_up((int)_plast.x, (int)_plast.y, 1);
        }
        if (t == _t1) { _t1 = _t2; _t2 = nil; }
        else _t2 = nil;
    }
    if (!_t1 && !_t2) { _mode = T_WAIT; ui_ev_leave(); }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self finish:touches cancelled:NO]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self finish:touches cancelled:YES]; }
@end

/* ---------- экран: игра во всю безопасную область, только альбомная ориентация ---------- */
@interface GameVC : UIViewController
@end

@implementation GameVC {
    GameView *_gv;
}
- (void)loadView
{
    UIView *root = [[UIView alloc] init];
    root.backgroundColor = UIColor.blackColor;
    _gv = [[GameView alloc] initWithFrame:CGRectZero];
    [root addSubview:_gv];
    self.view = root;
}
- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    UIEdgeInsets in = self.view.safeAreaInsets;
    _gv.frame = UIEdgeInsetsInsetRect(self.view.bounds, in);
}
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)opts
{
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    [[NSFileManager defaultManager] createDirectoryAtPath:docs withIntermediateDirectories:YES attributes:nil error:nil];
    snprintf(s_dataDir, sizeof s_dataDir, "%s", docs.fileSystemRepresentation);
    snprintf(s_resDir, sizeof s_resDir, "%s", NSBundle.mainBundle.resourcePath.fileSystemRepresentation);
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[GameVC alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}
- (void)applicationDidEnterBackground:(UIApplication *)app { ui_ev_close(); }
- (void)applicationWillTerminate:(UIApplication *)app { ui_ev_close(); }
@end

int main(int argc, char *argv[])
{
    /* swprintf в системной библиотеке Apple проводит строки через многобайтовую кодировку:
     * без UTF-8 любая кириллица в формате или в %ls обрывает строку */
    if (!setlocale(LC_CTYPE, "en_US.UTF-8")) setlocale(LC_CTYPE, "UTF-8");
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class));
    }
}
