#include <iostream>
#include <QRegularExpression>
#include <QString>

int main() {
    QString _output = "<pre><code class=\"language-cpp\">int x=0;</code></pre>";
    QRegularExpression re("<pre><code( class=\"language-(.*?)\")?>(.*?)</code></pre>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = re.match(_output);
    if (match.hasMatch()) {
        std::cout << "Match 1: " << match.captured(1).toStdString() << "\n";
        std::cout << "Match 2: " << match.captured(2).toStdString() << "\n";
        std::cout << "Match 3: " << match.captured(3).toStdString() << "\n";
    } else {
        std::cout << "No match\n";
    }
    return 0;
}
