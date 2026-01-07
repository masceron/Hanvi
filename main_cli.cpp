#include <QCoreApplication>
#include <QCommandLineParser>
#include <qdir.h>

#include "core/converter.h"
#include "core/dict.h"
#include "core/trie.h"

void writeStdOut(const QString& text)
{
    QTextStream out(stdout);
#if defined(Q_OS_WIN)
    out.setEncoding(QStringConverter::Utf8);
#endif
    out << text;
}

QString readStdIn()
{
    QTextStream in(stdin);
#if defined(Q_OS_WIN)
    in.setEncoding(QStringConverter::Utf8);
#endif
    return in.readAll();
}

int main(int argc, char* argv[])
{
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

    const QCommandLineOption output_option_folder(QStringList() << "o" << "output",
                                                  "Write all files to <folder>.", "folder");
    parser.addOption(output_option_folder);

    parser.process(app);

    qInfo() << "Loading dictionaries...";

    load_dict([&]
    {
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

            QStringList filters;
            filters << "*.txt";
            inDir.setNameFilters(filters);
            QFileInfoList files = inDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

            if (files.isEmpty())
            {
                qWarning() << "Warning: No .txt files found in" << inDir.absolutePath();
                QCoreApplication::exit();
            }

            qInfo() << "Processing" << files.size() << "file(s)...";

            int successCount = 0;

            for (const QFileInfo& file_info : files)
            {
                QFile in_file(file_info.absoluteFilePath());
                if (!in_file.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    qWarning() << "Skipping: Cannot open" << file_info.fileName();
                    continue;
                }

                QTextStream in(&in_file);
                in.setEncoding(QStringConverter::Utf8);
                QString content = in.readAll();
                in_file.close();

                QString result = convert_plain(content);

                QString out_name = file_info.baseName() + "_translated.txt";

                QString out_path = out_dir.filePath(out_name);

                QFile out_file(out_path);
                if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    qWarning() << "Skipping: Cannot write to" << out_name;
                    continue;
                }

                QTextStream out(&out_file);
                out.setEncoding(QStringConverter::Utf8);
                out << result;
                out_file.close();

                successCount++;
                qInfo() << "Converted:" << file_info.absoluteFilePath() << "->" << out_path << "." << successCount << "/" << files.size();
            }

            qInfo() << "Batch completed." << successCount << "/" << files.size() << "files processed.";
            QCoreApplication::exit();
        }
        QCoreApplication::exit();
    });

    return app.exec();
}
