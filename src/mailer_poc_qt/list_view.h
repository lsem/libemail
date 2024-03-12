#pragma once

#include <QListView>

class ListView : public QListView {
    Q_OBJECT
   public:
    explicit ListView(QWidget* parent = nullptr);
};
