

#include "LogHighlighter.hpp"

#define TO_EOL "(([\\s\\S]*)|([\\d\\D]*)|([\\w\\W]*))$"
#define REGEX_IPV6_ADDR \
    R"(\[\s*((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?\s*\])"
#define REGEX_IPV4_ADDR \
    R"((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5]))"
#define REGEX_PORT_NUMBER R"(([0-9]|[1-9]\d{1,3}|[1-5]\d{4}|6[0-5]{2}[0-3][0-5])*)"

namespace Qv2ray::ui {
    SyntaxHighlighter::SyntaxHighlighter(bool darkMode, QTextDocument *parent) : QSyntaxHighlighter(parent) {
        HighlightingRule rule;

        if (darkMode) {
            tcpudpFormat.setForeground(QColor(0xE2, 0x88, 0x6B));
            ipHostFormat.setForeground(QColor(0xDF, 0xC1, 0x84));
            warningFormat.setForeground(QColor(0xE5, 0xA6, 0x44));
            warningFormat2.setForeground(QColor(0xE5, 0xA6, 0x44));
        } else {
            ipHostFormat.setForeground(QColor(0x6B, 0x5B, 0x3E));
            tcpudpFormat.setForeground(QColor(0xA8, 0x4C, 0x2C));
            warningFormat.setForeground(QColor(0xB8, 0x73, 0x14));
            warningFormat2.setForeground(QColor(0xB8, 0x73, 0x14));
        }
        const QColor acceptedColor = darkMode ? QColor(0x5B, 0xB9, 0x82) : QColor(0x2E, 0x7D, 0x52);
        const QColor rejectedColor = darkMode ? QColor(0xE0, 0x6C, 0x60) : QColor(0xC0, 0x45, 0x3A);
        const QColor dateColor = darkMode ? QColor(0xD9, 0x77, 0x57) : QColor(0xB8, 0x57, 0x37);

        acceptedFormat.setForeground(acceptedColor);
        acceptedFormat.setFontItalic(true);
        acceptedFormat.setFontWeight(QFont::Bold);
        rule.pattern = QRegularExpression("\\saccepted\\s");
        rule.format = acceptedFormat;
        highlightingRules.append(rule);
        //
        rejectedFormat.setFontWeight(QFont::Bold);
        rejectedFormat.setBackground(QColor(224, 108, 96, 32));
        rejectedFormat.setForeground(rejectedColor);
        rule.pattern = QRegularExpression("\\srejected\\s" TO_EOL);
        rule.format = rejectedFormat;
        highlightingRules.append(rule);
        //
        dateFormat.setForeground(dateColor);
        rule.pattern = QRegularExpression("\\d\\d\\d\\d/\\d\\d/\\d\\d");
        rule.format = dateFormat;
        highlightingRules.append(rule);
        //
        timeFormat.setForeground(dateColor);
        rule.pattern = QRegularExpression("\\d\\d:\\d\\d:\\d\\d");
        rule.format = timeFormat;
        highlightingRules.append(rule);
        //
        debugFormat.setForeground(QColor(0x78, 0x76, 0x6F));
        rule.pattern = QRegularExpression("D[Ee][Bb][Uu][Gg].*?");
        rule.format = debugFormat;
        highlightingRules.append(rule);
        //
        infoFormat.setForeground(darkMode ? QColor(0xB0, 0xAE, 0xA5) : QColor(0x5E, 0x5D, 0x59));
        rule.pattern = QRegularExpression("I[Nn][Ff][Oo].*?");
        rule.format = infoFormat;
        highlightingRules.append(rule);
        //
        warningFormat.setFontWeight(QFont::Bold);
        warningFormat2.setFontWeight(QFont::Bold);
        rule.pattern = QRegularExpression("W[Aa][Rr][Nn].*?");
        rule.format = warningFormat2;
        highlightingRules.append(rule);
        //
        rule.pattern = QRegularExpression("E[Rr][Rr][Oo][Rr].*?");
        rule.format = rejectedFormat;
        highlightingRules.append(rule);

        //
        v2rayComponentFormat.setForeground(acceptedColor);
        rule.pattern = QRegularExpression(R"( (\w+\/)+\w+: )");
        rule.format = v2rayComponentFormat;
        highlightingRules.append(rule);
        //
        rule.pattern = QRegularExpression(">>>>+");
        rule.format = warningFormat;
        highlightingRules.append(rule);
        //
        rule.pattern = QRegularExpression("<<<<+");
        rule.format = warningFormat;
        highlightingRules.append(rule);
        //
        rule.pattern = QRegularExpression("error");
        rule.format = rejectedFormat;
        highlightingRules.append(rule);

        {
            // IP IPv6 Host;
            rule.pattern = QRegularExpression(REGEX_IPV4_ADDR ":" REGEX_PORT_NUMBER);
            rule.pattern.setPatternOptions(QRegularExpression::ExtendedPatternSyntaxOption);
            rule.format = ipHostFormat;
            highlightingRules.append(rule);
            //
            rule.pattern = QRegularExpression(REGEX_IPV6_ADDR ":" REGEX_PORT_NUMBER);
            rule.pattern.setPatternOptions(QRegularExpression::ExtendedPatternSyntaxOption);
            rule.format = ipHostFormat;
            highlightingRules.append(rule);
            //
            rule.pattern = QRegularExpression("([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}(/|):" REGEX_PORT_NUMBER);
            rule.pattern.setPatternOptions(QRegularExpression::PatternOption::ExtendedPatternSyntaxOption);
            rule.format = ipHostFormat;
            highlightingRules.append(rule);
        }

        for (const auto &pattern: {"tcp:", "udp:"}) {
            tcpudpFormat.setFontWeight(QFont::Bold);
            rule.pattern = QRegularExpression(pattern);
            rule.format = tcpudpFormat;
            highlightingRules.append(rule);
        }
    }

    void SyntaxHighlighter::highlightBlock(const QString &text) {
        for (const HighlightingRule &rule: highlightingRules) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);

            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }

        setCurrentBlockState(0);
    }
} // namespace Qv2ray::ui
