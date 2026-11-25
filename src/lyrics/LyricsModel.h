// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "LyricsDocument.h"

#include <QAbstractListModel>
#include <QVariantList>

// The current track's lyric lines, for QML views. One row per line.
class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY documentChanged)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY documentChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY documentChanged)
    Q_PROPERTY(bool hasWordTiming READ hasWordTiming NOTIFY documentChanged)
    Q_PROPERTY(bool hasTranslation READ hasTranslation NOTIFY documentChanged)
    Q_PROPERTY(QString source READ source NOTIFY documentChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY documentChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // Bumped on every document change, so views can restart transitions.
    Q_PROPERTY(int revision READ revision NOTIFY documentChanged)

public:
    enum Role {
        StartMsRole = Qt::UserRole + 1,
        EndMsRole,
        TextRole,
        TranslationRole,
        RomanizationRole,
        WordsRole,
        IsInstrumentalRole,
        WordsEstimatedRole,
    };
    Q_ENUM(Role)

    explicit LyricsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDocument(const LyricsDocument &doc);
    void clear();
    void setLoading(bool loading);
    const LyricsDocument &document() const { return m_doc; }

    int count() const { return int(m_doc.lines.size()); }
    bool hasLyrics() const { return !m_doc.lines.isEmpty(); }
    bool synced() const { return m_doc.synced; }
    bool hasWordTiming() const { return m_doc.hasWordTiming; }
    bool hasTranslation() const { return m_doc.hasTranslation; }
    QString source() const { return m_doc.source; }
    QString sourcePath() const { return m_doc.sourcePath; }
    bool loading() const { return m_loading; }
    int revision() const { return m_revision; }

    // A line as a JS object: {startMs, endMs, text, translation, romanization, words, isInstrumental}.
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void documentChanged();
    void loadingChanged();

private:
    LyricsDocument m_doc;
    QList<QVariantList> m_wordsCache;
    bool m_loading = false;
    int m_revision = 0;
};
