#include <QApplication>
#include <QCommandLineParser>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("NyraPlayer");
    QApplication::setApplicationName("Nyra Player");

    QCommandLineParser parser;
    parser.setApplicationDescription("Nyra Player - VLC-derived, power-aware media player");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Media file or URL to open on launch (optional)");
    parser.process(app);

    MainWindow window;
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
        QMetaObject::invokeMethod(&window, "onOpenFileFromArgv", Qt::QueuedConnection,
                                   Q_ARG(QString, args.first()));

    return app.exec();
}
