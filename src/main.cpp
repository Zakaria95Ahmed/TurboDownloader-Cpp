/**
 * @file main.cpp
 * @brief OpenIDM Application Entry Point
 * 
 * Initializes the Qt application, download engine, and QML UI.
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QDir>

#include "openidm/engine/DownloadManager.h"
#include "openidm/viewmodel/DownloadListModel.h"
#include "openidm/viewmodel/DownloadViewModel.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
    // Enable high DPI scaling
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    QGuiApplication app(argc, argv);
    
    // Application metadata
    app.setOrganizationName(QStringLiteral("OpenIDM"));
    app.setOrganizationDomain(QStringLiteral("openidm.org"));
    app.setApplicationName(QStringLiteral("OpenIDM"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    
    // Set application icon
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/icons/app_icon.svg")));
    
    // Load custom fonts
    int fontId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/resources/fonts/Inter-Regular.ttf"));
    if (fontId != -1) {
        QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);
        QFont defaultFont(family);
        defaultFont.setPixelSize(14);
        app.setFont(defaultFont);
    }
    
    // Set Qt Quick style
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    
    // Initialize curl globally (thread-safe initialization)
    // Note: In production, use curl_global_init(CURL_GLOBAL_ALL)
    
    // Initialize download manager
    if (!OpenIDM::DownloadManager::initialize(&app)) {
        qCritical() << "Failed to initialize DownloadManager";
        return 1;
    }
    
    // Create view models
    auto& manager = OpenIDM::DownloadManager::instance();
    OpenIDM::DownloadListModel downloadListModel(&manager);
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Register types with QML
    qmlRegisterUncreatableType<OpenIDM::DownloadTask>(
        "OpenIDM", 1, 0, "DownloadTask",
        QStringLiteral("DownloadTask cannot be created from QML"));
    
    // Expose C++ objects to QML
    engine.rootContext()->setContextProperty(
        QStringLiteral("downloadManager"), &manager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("downloadListModel"), &downloadListModel);
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "Failed to load QML";
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    // Run application
    int result = app.exec();
    
    // Cleanup
    OpenIDM::DownloadManager::shutdown();
    
    return result;
}
