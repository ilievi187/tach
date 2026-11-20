// tach — an AMD GPU panel for Omarchy.

#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include <cstdio>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    const QString first = args.value(0);
    const bool headless = first == "status" || first == "set-level";

    // `--screenshot out.png` renders the window offscreen and saves it, so the
    // README image can be regenerated without touching the desktop.
    const int screenshotIndex = args.indexOf("--screenshot");
    const bool screenshotting = screenshotIndex >= 0 && screenshotIndex + 1 < args.size();

    // Headless commands must not need a display.
    if (headless || screenshotting) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QUICK_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("tach");
    app.setApplicationVersion("0.1.0");

    // Matches the window to tach.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("tach");
    app.setWindowIcon(QIcon::fromTheme("tach"));

    if (headless)
        return runCli(args);

    if (args.contains("--version")) {
        std::printf("tach %s\n", qPrintable(app.applicationVersion()));
        return 0;
    }

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    // `tach --card 3` or `--card=/sys/class/drm/card3/device` for machines
    // where the discrete card is not the one with most VRAM.
    const int cardIndex = args.indexOf("--card");
    if (cardIndex >= 0 && cardIndex + 1 < args.size()) {
        const QString value = args.value(cardIndex + 1);
        const QString path = value.startsWith('/') ? value
                                                   : QString("/sys/class/drm/card%1/device").arg(value);
        backend.setCardPath(path);
    }

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (screenshotting) {
        const QString path = args.value(screenshotIndex + 1);
        const int delayIndex = args.indexOf("--delay");
        const int delay = delayIndex >= 0 ? qMax(500, args.value(delayIndex + 1).toInt()) : 1200;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

        // `--size 466x508` reproduces the size a tiling compositor hands out.
        const int sizeIndex = args.indexOf("--size");
        if (window && sizeIndex >= 0 && sizeIndex + 1 < args.size()) {
            const QStringList parts = args.value(sizeIndex + 1).split('x');
            if (parts.size() == 2)
                window->resize(parts.at(0).toInt(), parts.at(1).toInt());
        }

        // Long enough for the first poll and a couple of history points; a
        // longer --delay lets a documentation shot show a filled history line.
        QTimer::singleShot(delay, &app, [window, path]() {
            if (window) {
                const QImage image = window->grabWindow();
                if (image.save(path))
                    std::printf("wrote %s\n", qPrintable(path));
                else
                    std::fprintf(stderr, "could not write %s\n", qPrintable(path));
            }
            QGuiApplication::quit();
        });
    }

    return app.exec();
}
