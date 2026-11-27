#pragma once
#include <QApplication>

QString dict_db_path();

// Whether conversions segment by word frequency (see hanvi::Segmentation); kept in settings.json.
[[nodiscard]] bool load_frequency_segmentation();
void save_frequency_segmentation(bool enabled);

void init_style(QApplication &app);
