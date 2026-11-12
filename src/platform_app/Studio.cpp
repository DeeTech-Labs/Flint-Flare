#include "Studio.h"
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
Studio::Studio(QObject *parent) : QObject(parent) {
    for (auto name : {"compile_ignite.py", "build_package.py", "flim.py"})
        QFile::copy(QString(":/sdk/") + name, tools_.path() + "/" + name);
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        message_ = code == 0 && status == QProcess::NormalExit
            ? "Ignite собран"
            : QString::fromUtf8(process_.readAllStandardError()).right(4096);
        if (code != 0)
            output_ = QUrl();
        emit changed();
    });
    connect(&process_, &QProcess::errorOccurred, this, [this] {
        message_ = process_.errorString();
        emit changed();
    });
}
void Studio::build(const QUrl &source, const QUrl &output) {
    if (busy())
        return;
    if (!source.isLocalFile() || !output.isLocalFile() ||
        !QFileInfo::exists(source.toLocalFile())) {
        message_ = "Выберите JSON-проект и выходной .ignit";
        emit changed();
        return;
    }
    output_ = output;
    message_ = "Компиляция…";
#ifdef Q_OS_WIN
    // Windows commonly provides a python3 App Execution Alias even when the
    // installed interpreter is python.exe. Prefer the installed command.
    QString python = QStandardPaths::findExecutable("python");
    if (python.isEmpty())
        python = QStandardPaths::findExecutable("python3");
#else
    QString python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty())
        python = QStandardPaths::findExecutable("python");
#endif
    if (python.isEmpty()) {
        message_ = tr("Python 3 is required to build Ignite packages.");
        emit changed();
        return;
    }
    process_.start(
        python, {tools_.path() + "/compile_ignite.py", source.toLocalFile(), output.toLocalFile()});
    emit changed();
}
