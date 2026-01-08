#include <iostream>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QElapsedTimer>

#include "core/converter.h"
#include "core/dict.h"
#include "core/trie.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

void write_std_out(const QString& text)
{
    QTextStream out(stdout);
#if defined(Q_OS_WIN)
    out.setEncoding(QStringConverter::Utf8);
#endif
    out << text;
}

int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(65001);
#endif

    const QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Converter-CLI");
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

    parser.process(app);

    QElapsedTimer timer_dict;
    timer_dict.start();

    std::cout << "Loading dictionaries..." << std::flush;

    load_dict([&]
    {
        std::cout << " " << static_cast<double>(timer_dict.elapsed()) / 1000 << "s." << std::endl;
        std::flush(std::cout);
        QString input_text;

        if (parser.isSet(input_option_folder) || parser.isSet(output_option_folder))
        {
            if (!parser.isSet(input_option_folder) || !parser.isSet(output_option_folder))
            {
                qCritical() << "Error: When using folder mode, both -fi and -fo must be specified.";
                QCoreApplication::exit();
            }

            QDir inDir(parser.value(input_option_folder));
            const QDir out_dir(parser.value(output_option_folder));

            if (!inDir.exists())
            {
                qCritical() << "Error: Input folder does not exist:" << inDir.absolutePath();
                QCoreApplication::exit();
            }

            if (!out_dir.exists())
            {
                if (!out_dir.mkpath("."))
                {
                    qCritical() << "Error: Could not create output folder:" << out_dir.absolutePath();
                    QCoreApplication::exit();
                }
            }

            if (const auto set_specified = parser.value(name_set_used); !set_specified.isEmpty())
            {
                const auto set_chosen = std::ranges::find_if(name_sets, [&](const NameSet& name_set)
                {
                    return name_set.title.compare(set_specified, Qt::CaseInsensitive) == 0;
                });
                if (set_chosen == name_sets.end())
                {
                    qWarning().nospace() << "Cannot find the specified nameset: " << set_specified << ". Ignoring...";
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
            QFileInfoList files = inDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

            if (files.isEmpty())
            {
                qWarning() << "Warning: No .txt files found in" << inDir.absolutePath();
                QCoreApplication::exit();
            }

            std::cout << "Processing " << files.size() << " file(s)..." << std::endl;

            QMutex console_mutex;

            auto process_file = [&](const QFileInfo& file_info)
            {
                QElapsedTimer timer;
                timer.start();

                QFile in_file(file_info.absoluteFilePath());
                if (!in_file.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    qWarning() << "Skipping: Cannot open" << file_info.fileName();
                    return;
                }

                QTextStream in(&in_file);
                in.setEncoding(QStringConverter::Utf8);
                const QString content = in.readAll();
                in_file.close();

                const QString result = convert_plain(content);
                const QString out_name = file_info.baseName() + "_converted.txt";
                const QString out_path = out_dir.filePath(out_name);

                QFile out_file(out_path);
                if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    qWarning() << "Skipping: Cannot write to" << out_name;
                    return;
                }

                QTextStream out(&out_file);
                out.setEncoding(QStringConverter::Utf8);
                out << result;
                out_file.close();

                QMutexLocker locker(&console_mutex);

                QString logMsg;
                QTextStream ts(&logMsg);

                ts << "Converted "
                    << file_info.absoluteFilePath()
                    << " -> "
                    << out_path
                    << ": "
                    << QString::number(static_cast<double>(timer.elapsed()) / 1000.0, 'f', 3)
                    << "s.\n";

                write_std_out(logMsg);
            };

            QtConcurrent::blockingMap(files, process_file);

            QCoreApplication::exit(0);
        }
    });

    return QCoreApplication::exec();
}
