#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#pragma once

#include <QString>
#include <QMap>
#include <QHash>

#include <QObject>



class LogTableModel;

class LanguageManager : public QObject {
    Q_OBJECT
public:
    enum class Lang { System, Korean, English };

    static LanguageManager& inst();
    QString t(const QString& key) const;
    void set(Lang l);
    Lang current() const;


    using Lang = LanguageManager::Lang;


signals:
    void languageChanged(LanguageManager::Lang);
private:
    LanguageManager();
    Lang cur_ = Lang::System;
    QHash<QString, QString> ko_;
    QHash<QString, QString> dict_;
};


#endif
