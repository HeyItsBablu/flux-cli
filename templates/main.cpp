#include <flux/flux.hpp>

class MyApp : public Widget {
public:
    WidgetPtr build() override {
        return Scaffold(
            AppBar("My App"),
            Center(Text("Hello World")));
    }
};

WidgetPtr createApp(FluxUI *app) {
    return FluxApp(
        "My App",
        std::make_shared<MyApp>(),
        AppTheme::light(),
        false, 900, 700, false, false);
}