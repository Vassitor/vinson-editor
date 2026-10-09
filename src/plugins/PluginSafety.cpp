#include "plugins/PluginSafety.h"

#include <QCoreApplication>
#include <QSet>
#include <QStringList>
#include <algorithm>

namespace vinson {
namespace {
struct Token {
    QString text;
    int offset;
    bool literal = false;
    bool escaped = false;
};

QString safetyText(const char* source)
{
    return QCoreApplication::translate("vinson::PluginSafety", source);
}

// Keep locations and decoded string tokens (for computed property access),
// while ignoring comments and regex bodies. Templates retain their expressions.
QVector<Token> tokenize(const QString& script)
{
    QVector<Token> tokens;
    QVector<int> templateBraces;
    bool templateText = false;
    int i = 0;
    const int size = static_cast<int>(script.size());
    auto unicodeEscape = [&script, size](int& position, QChar& decoded) {
        if (position + 5 >= size || script[position] != QLatin1Char('\\')
            || script[position + 1] != QLatin1Char('u')) return false;
        bool valid = false;
        const auto value = script.mid(position + 2, 4).toUShort(&valid, 16);
        if (!valid) return false;
        decoded = QChar(value);
        position += 6;
        return true;
    };
    while (i < size) {
        const int start = i;
        const auto c = script[i];
        if (templateText) {
            if (c == QLatin1Char('\\')) { i += 2; continue; }
            if (c == QLatin1Char('`')) { templateText = false; ++i; continue; }
            if (c == QLatin1Char('$') && i + 1 < size && script[i + 1] == QLatin1Char('{')) {
                templateBraces.append(0);
                templateText = false;
                i += 2;
                continue;
            }
            ++i;
            continue;
        }
        if (c.isSpace()) { ++i; continue; }
        if (script.mid(i, 2) == QLatin1String("//")) {
            while (i < size && script[i] != QLatin1Char('\n') && script[i] != QLatin1Char('\r')
                && script[i] != QChar(0x2028) && script[i] != QChar(0x2029)) ++i;
            continue;
        }
        if (script.mid(i, 2) == QLatin1String("/*")) {
            const int end = static_cast<int>(script.indexOf(QStringLiteral("*/"), i + 2));
            i = end < 0 ? size : end + 2;
            continue;
        }
        if (c == QLatin1Char('`')) { templateText = true; ++i; continue; }
        if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            const auto quote = c;
            QString value;
            ++i;
            while (i < size && script[i] != quote) {
                QChar decoded;
                if (unicodeEscape(i, decoded)) { value += decoded; continue; }
                if (script[i] == QLatin1Char('\\') && i + 1 < size) {
                    ++i;
                    if (script[i] == QLatin1Char('x') && i + 2 < size) {
                        bool valid = false;
                        const auto hex = script.mid(i + 1, 2).toUShort(&valid, 16);
                        if (valid) { value += QChar(hex); i += 3; continue; }
                    }
                }
                value += script[i++];
            }
            if (i < size) ++i;
            tokens.append({value, start, true, false});
            continue;
        }
        if (c == QLatin1Char('/')) {
            const QString previous = tokens.isEmpty() ? QString() : tokens.last().text;
            const bool regex = tokens.isEmpty() || QStringList{"(", "=", ":", ",", "!", "[", "{", ";",
                "return", "throw", "case", "yield", "void", "typeof", "delete", "+", "-", "*", "%",
                "&", "|", "^", "~", "?", "<", ">"}.contains(previous);
            if (regex) {
                ++i;
                bool characterClass = false;
                while (i < size) {
                    if (script[i] == QLatin1Char('\\')) { i += 2; continue; }
                    if (script[i] == QLatin1Char('[')) characterClass = true;
                    if (script[i] == QLatin1Char(']')) characterClass = false;
                    if (script[i++] == QLatin1Char('/') && !characterClass) break;
                }
                while (i < size && script[i].isLetter()) ++i;
                tokens.append({QStringLiteral("<regex>"), start, true, false});
                continue;
            }
        }
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$') || c == QLatin1Char('\\')) {
            QString value;
            bool escaped = false;
            while (i < size) {
                QChar decoded;
                if (unicodeEscape(i, decoded)) { value += decoded; escaped = true; continue; }
                const auto part = script[i];
                if (!part.isLetterOrNumber() && part != QLatin1Char('_') && part != QLatin1Char('$')) break;
                value += part;
                ++i;
            }
            if (i == start) ++i;
            tokens.append({value, start, false, escaped});
            continue;
        }
        if (!templateBraces.isEmpty()) {
            if (c == QLatin1Char('{')) ++templateBraces.last();
            if (c == QLatin1Char('}') && templateBraces.last()-- == 0) {
                templateBraces.removeLast();
                templateText = true;
                ++i;
                continue;
            }
        }
        tokens.append({QString(c), start, false, false});
        ++i;
    }
    return tokens;
}
} // namespace

QVector<PluginRisk> scanPluginScript(const QString& script, const QString& commandId,
                                    const QString& commandTitle)
{
    QVector<PluginRisk> risks;
    const auto tokens = tokenize(script);
    QVector<int> lineStarts{0};
    for (int offset = 0; offset < script.size(); ++offset) {
        if (script[offset] == QLatin1Char('\r')) {
            if (offset + 1 < script.size() && script[offset + 1] == QLatin1Char('\n')) ++offset;
            lineStarts.append(offset + 1);
        } else if (script[offset] == QLatin1Char('\n') || script[offset] == QChar(0x2028)
            || script[offset] == QChar(0x2029)) lineStarts.append(offset + 1);
    }
    QSet<QString> reported;
    auto add = [&](const Token& token, const char* category, const char* explanation) {
        if (risks.size() >= 64) return;
        const int line = static_cast<int>(std::upper_bound(lineStarts.cbegin(), lineStarts.cend(), token.offset)
            - lineStarts.cbegin());
        const QString key = QString::fromLatin1(category) + QString::number(line);
        if (reported.contains(key)) return;
        reported.insert(key);
        const int start = lineStarts[line - 1];
        const int end = line < lineStarts.size() ? lineStarts[line] : static_cast<int>(script.size());
        const int excerptStart = std::max(start, token.offset - 80);
        const int excerptEnd = std::min(end, excerptStart + 240);
        QString evidence = script.mid(excerptStart, excerptEnd - excerptStart).trimmed();
        if (excerptStart > start) evidence.prepend(QChar(0x2026));
        if (excerptEnd < end) evidence.append(QChar(0x2026));
        risks.append({commandId, commandTitle, line, safetyText(category), safetyText(explanation),
            evidence});
    };
    auto text = [&tokens](qsizetype i) { return i >= 0 && i < tokens.size() ? tokens[i].text : QString(); };
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        const bool property = token.literal && text(i - 1) == QLatin1String("[") && text(i + 1) == QLatin1String("]");
        if (token.literal && !property) continue;
        if (text(i - 1) == QLatin1String("typeof")) continue;
        if (token.escaped) add(token, "Obfuscated code",
            "Escaped identifiers can hide the APIs being called and make review harder.");
        if (QStringList{"eval", "Function", "constructor"}.contains(token.text))
            add(token, "Dynamic code execution",
                "Code can be generated or evaluated at runtime, hiding behavior from this static check. Generated code can still read or change the provided document text.");
        if (QStringList{"require", "import", "process", "QProcess", "child_process", "execFile", "spawn", "system"}.contains(token.text))
            add(token, "System or module access",
                "This code refers to modules or system commands that could execute programs or access the computer in a host with those APIs. These host APIs are not provided by the current plugin runtime.");
        if (QStringList{"fetch", "XMLHttpRequest", "WebSocket", "sendBeacon", "QTcpSocket", "QNetworkAccessManager"}.contains(token.text))
            add(token, "Network access",
                "This code refers to network APIs that could send document text to a remote server in a host with those APIs. Network APIs are not provided by the current plugin runtime.");
        if (QStringList{"readFile", "readFileSync", "writeFile", "writeFileSync", "unlink", "unlinkSync", "rmSync", "rmdirSync", "QFile"}.contains(token.text))
            add(token, "File access or deletion",
                "This code refers to file APIs that could read, overwrite or delete files in a host with those APIs. File APIs are not provided by the current plugin runtime.");
        if (QStringList{"atob", "fromCharCode", "fromCodePoint"}.contains(token.text)
            && (text(i + 1) == QLatin1String("(") || property))
            add(token, "Encoded content",
                "Decoded text may conceal executable code or destinations. Review how the decoded value is used; decoding alone is not necessarily harmful.");
        if ((token.text == QLatin1String("while") && text(i + 1) == QLatin1String("(")
                && QStringList{"true", "1"}.contains(text(i + 2)) && text(i + 3) == QLatin1String(")"))
            || (token.text == QLatin1String("for") && text(i + 1) == QLatin1String("(")
                && text(i + 2) == QLatin1String(";") && text(i + 3) == QLatin1String(";")))
            add(token, "Potential infinite loop",
                "A loop without a limiting condition can consume CPU or memory. Cancellation and the time limit reduce CPU stalls but cannot guarantee recovery from memory exhaustion. Check for a reachable break or return.");
    }
    return risks;
}
} // namespace vinson
