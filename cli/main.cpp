#include <print>
#include <iostream>
#include <QElapsedTimer>
#include <QCommandLineParser>
#include <QDir>
#include <QtConcurrentMap>

#include "core/converter.h"
#include "core/dict.h"
#include "core/structures.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(65001);
#endif

    const QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Hanvi-CLI");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Convert Chinese text to Vietnamese via CLI.");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption input_option_folder(QStringList() << "i" << "input",
                                                 "Read all files from <folder>.", "folder");
    parser.addOption(input_option_folder);

    const QCommandLineOption name_set_used(QStringList() << "n" << "nameset",
                                           "Use the specified nameset if exists.", "nameset");
    parser.addOption(name_set_used);

    const QCommandLineOption output_option_folder(QStringList() << "o" << "output",
                                                   "Write all files to <folder>.", "folder");
    parser.addOption(output_option_folder);

    const QCommandLineOption job_number(QStringList() << "j" << "jobs",
                                        "Number of conversion jobs, default 0 (all).", "jobs", "0");
    parser.addOption(job_number);

    if (!parser.parse(QCoreApplication::arguments()))
    {
        std::println(stderr, "Error: {}", parser.errorText().toStdString());
        return 1;
    }

    if (parser.isSet("help"))
    {
        std::println("{}", parser.helpText().toStdString());
        return 0;
    }

    if (parser.isSet("version"))
    {
        std::println("Hanvi-CLI 1.0");
        return 0;
    }

    if (!parser.isSet(input_option_folder) || !parser.isSet(output_option_folder))
    {
        std::println(stderr, "Error: Both -i and -o must be specified.");
        return 1;
    }

    QDir inDir(parser.value(input_option_folder));
    const QDir out_dir(parser.value(output_option_folder));

    if (!inDir.exists())
    {
        std::println(stderr, "Error: Input folder does not exist: {}", inDir.absolutePath().toStdString());
        return 1;
    }

    if (!out_dir.exists())
    {
        if (!out_dir.mkpath("."))
        {
            std::println(stderr, "Error: Could not create output folder: {}", out_dir.absolutePath().toStdString());
            return 1;
        }
    }

    QElapsedTimer timer_dict;
    timer_dict.start();

    std::print("Loading dictionaries...");
    std::fflush(stdout);

    if (const QString opencc = QCoreApplication::applicationDirPath() + "/opencc/t2s.json"; QFile::exists(opencc))
    {
        init_opencc(opencc.toLocal8Bit().toStdString());
    }
    load_dict();

    std::println(" {}s.", static_cast<double>(timer_dict.elapsed()) / 1000);
    std::fflush(stdout);

    if (const int jobs = parser.value(job_number).toInt(); jobs > 0)
    {
        QThreadPool::globalInstance()->setMaxThreadCount(jobs);
    }

    if (const auto set_specified = parser.value(name_set_used); !set_specified.isEmpty())
    {
        const auto set_chosen = std::ranges::find_if(name_sets, [&](const NameSet& name_set)
        {
            return QString::fromUtf16(name_set.title.data(), static_cast<qsizetype>(name_set.title.size())).compare(set_specified, Qt::CaseInsensitive) == 0;
        });
        if (set_chosen == name_sets.end())
        {
            std::println(stderr, "Cannot find the specified nameset: {}. Ignoring...", set_specified.toStdString());
        }
        else
        {
            const auto& [index, title] = *set_chosen;
            load_name_set(index);
        }
    }

    QStringList filters;
    filters << "*.txt";
    inDir.setNameFilters(filters);
    const QFileInfoList files = inDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    if (files.isEmpty())
    {
        std::println("Warning: No .txt files found in {}", inDir.absolutePath().toStdString());
        return 0;
    }

    std::println("Processing {} files.", files.size());
    std::fflush(stdout);

    QMutex console_mutex;

    auto process_file = [&](const QFileInfo& file_info)
    {
        QElapsedTimer timer;
        timer.start();

        QFile in_file(file_info.absoluteFilePath());
        if (!in_file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QMutexLocker locker(&console_mutex);
            std::println(stderr, "Skipping: Cannot open {}", file_info.fileName().toStdString());
            return;
        }

        QTextStream in(&in_file);
        in.setEncoding(QStringConverter::Utf8);
        const QString content = in.readAll();
        in_file.close();

        const QString result = QString::fromStdU16String(convert_plain(normalize_text(content.toStdU16String())));
        const QString out_name = file_info.baseName() + "_converted.txt";
        const QString out_path = out_dir.filePath(out_name);

        QFile out_file(out_path);
        if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMutexLocker locker(&console_mutex);
            std::println(stderr, "Skipping: Cannot write to {}", out_name.toStdString());
            return;
        }

        QTextStream out(&out_file);
        out.setEncoding(QStringConverter::Utf8);
        out << result;
        out_file.close();

        QMutexLocker locker(&console_mutex);
        std::println("Converted {} -> {}: {:.3f}s.",
                     file_info.absoluteFilePath().toStdString(),
                     out_path.toStdString(),
                     static_cast<double>(timer.elapsed()) / 1000.0);
        std::fflush(stdout);
    };

    QtConcurrent::blockingMap(files, process_file);

    return 0;
}
