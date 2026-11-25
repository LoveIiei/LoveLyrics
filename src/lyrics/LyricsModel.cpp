// SPDX-License-Identifier: AGPL-3.0-or-later
#include "LyricsModel.h"

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LyricsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant LyricsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const LyricLine &line = m_doc.lines.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole: return line.text;
    case StartMsRole: return line.startMs;
    case EndMsRole: return line.endMs;
    case TranslationRole: return line.translation;
    case RomanizationRole: return line.romanization;
    case WordsRole: return m_wordsCache.at(index.row());
    case IsInstrumentalRole: return line.isInstrumental;
    case WordsEstimatedRole: return line.wordsEstimated;
    }
    return {};
}

QHash<int, QByteArray> LyricsModel::roleNames() const
{
    return {
        {StartMsRole, "startMs"},
        {EndMsRole, "endMs"},
        {TextRole, "text"},
        {TranslationRole, "translation"},
        {RomanizationRole, "romanization"},
        {WordsRole, "words"},
        {IsInstrumentalRole, "isInstrumental"},
        {WordsEstimatedRole, "wordsEstimated"},
    };
}

void LyricsModel::setDocument(const LyricsDocument &doc)
{
    beginResetModel();
    m_doc = doc;
    m_wordsCache.clear();
    m_wordsCache.reserve(m_doc.lines.size());
    for (const auto &line : std::as_const(m_doc.lines)) {
        QVariantList words;
        words.reserve(line.words.size());
        for (const auto &w : line.words)
            words.append(QVariantMap{{"text", w.text}, {"startMs", w.startMs}, {"endMs", w.endMs}});
        m_wordsCache.append(words);
    }
    ++m_revision;
    endResetModel();
    emit documentChanged();
}

void LyricsModel::clear()
{
    setDocument({});
}

void LyricsModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

QVariantMap LyricsModel::get(int row) const
{
    if (row < 0 || row >= count())
        return {};
    const LyricLine &line = m_doc.lines.at(row);
    return {
        {"startMs", line.startMs},
        {"endMs", line.endMs},
        {"text", line.text},
        {"translation", line.translation},
        {"romanization", line.romanization},
        {"words", m_wordsCache.at(row)},
        {"isInstrumental", line.isInstrumental},
        {"wordsEstimated", line.wordsEstimated},
    };
}
