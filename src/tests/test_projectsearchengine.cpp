#include <QtTest/QtTest>

#include "utils/projectsearch/projectsearchengine.h"

class TestProjectSearchEngine : public QObject
{
    Q_OBJECT

private slots:
    void binarySample();
    void skipDirs();
    void pathSkipped();
    void plainMatch();
    void wholeWord();
    void regexMatch();
    void invalidRegex();
};

void TestProjectSearchEngine::binarySample()
{
    QVERIFY(ProjectSearchEngine::isProbableBinarySample(QByteArray("abc\0def", 7)));
    QVERIFY(!ProjectSearchEngine::isProbableBinarySample(QByteArrayLiteral("hello")));
}

void TestProjectSearchEngine::skipDirs()
{
    QVERIFY(ProjectSearchEngine::shouldSkipDirectoryName(QStringLiteral(".git")));
    QVERIFY(ProjectSearchEngine::shouldSkipDirectoryName(QStringLiteral("BUILD")));
    QVERIFY(!ProjectSearchEngine::shouldSkipDirectoryName(QStringLiteral("src")));
}

void TestProjectSearchEngine::pathSkipped()
{
#if defined(Q_OS_WIN)
    const QString root(QStringLiteral("C:/cremniy_search_test_root"));
    const QString file(QStringLiteral("C:/cremniy_search_test_root/build/x.txt"));
#else
    const QString root(QStringLiteral("/cremniy_search_test_root"));
    const QString file(QStringLiteral("/cremniy_search_test_root/build/x.txt"));
#endif
    QVERIFY(ProjectSearchEngine::pathContainsSkippedDirectory(root, file));
}

void TestProjectSearchEngine::plainMatch()
{
    ProjectSearchOptions o;
    o.query = QStringLiteral("foo");
    o.caseSensitive = false;
    o.wholeWord = false;
    o.useRegex = false;
    const auto re = ProjectSearchEngine::buildLineMatcher(o);
    QVERIFY(re.has_value());
    QVERIFY(ProjectSearchEngine::lineMatches(QStringLiteral("hello foobar"), *re));
}

void TestProjectSearchEngine::wholeWord()
{
    ProjectSearchOptions o;
    o.query = QStringLiteral("foo");
    o.caseSensitive = false;
    o.wholeWord = true;
    o.useRegex = false;
    const auto re = ProjectSearchEngine::buildLineMatcher(o);
    QVERIFY(re.has_value());
    QVERIFY(!ProjectSearchEngine::lineMatches(QStringLiteral("foobar"), *re));
    QVERIFY(ProjectSearchEngine::lineMatches(QStringLiteral("foo bar"), *re));
}

void TestProjectSearchEngine::regexMatch()
{
    ProjectSearchOptions o;
    o.query = QStringLiteral("a+b");
    o.caseSensitive = true;
    o.wholeWord = false;
    o.useRegex = true;
    const auto re = ProjectSearchEngine::buildLineMatcher(o);
    QVERIFY(re.has_value());
    QVERIFY(ProjectSearchEngine::lineMatches(QStringLiteral("xxaaab"), *re));
}

void TestProjectSearchEngine::invalidRegex()
{
    ProjectSearchOptions o;
    o.query = QStringLiteral("(");
    o.useRegex = true;
    const auto re = ProjectSearchEngine::buildLineMatcher(o);
    QVERIFY(!re.has_value());
}

QTEST_MAIN(TestProjectSearchEngine)
#include "test_projectsearchengine.moc"
