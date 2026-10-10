#pragma once

#include <QDialog>

class Statistics;

class StatisticsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatisticsDialog(const Statistics& statistics, QWidget* parent = nullptr);
};
