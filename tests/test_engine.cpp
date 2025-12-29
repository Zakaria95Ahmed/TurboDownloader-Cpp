/**
 * @file test_engine.cpp
 * @brief Unit tests for download engine components
 */

#include <QtTest>

#include "engine/DownloadTypes.h"

using namespace OpenIDM;

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void testSegmentInfoProgress();
    void testSegmentInfoRemaining();
    void testSegmentInfoCanSplit();
    void testDownloadInfoProgress();
    void testDownloadInfoEta();
    void testDownloadStatsFormatSpeed();
    void testDownloadStatsFormatEta();
};

void TestEngine::testSegmentInfoProgress()
{
    SegmentInfo seg;
    seg.startByte = 0;
    seg.endByte = 999;
    seg.downloadedBytes = 500;

    QCOMPARE(seg.progress(), 50.0);
}

void TestEngine::testSegmentInfoRemaining()
{
    SegmentInfo seg;
    seg.startByte = 0;
    seg.endByte = 999;
    seg.downloadedBytes = 300;

    QCOMPARE(seg.remainingBytes(), 700);
}

void TestEngine::testSegmentInfoCanSplit()
{
    SegmentInfo seg;
    seg.startByte = 0;
    seg.endByte = Config::MIN_SPLIT_SIZE * 4 - 1;
    seg.downloadedBytes = 0;

    QVERIFY(seg.canSplit());

    seg.downloadedBytes = Config::MIN_SPLIT_SIZE * 3;
    QVERIFY(!seg.canSplit());
}

void TestEngine::testDownloadInfoProgress()
{
    DownloadInfo info;
    info.totalSize = 1000;
    info.downloadedBytes = 250;

    QCOMPARE(info.progress(), 25.0);
}

void TestEngine::testDownloadInfoEta()
{
    DownloadInfo info;
    info.totalSize = 1000000;
    info.downloadedBytes = 500000;
    info.averageSpeed = 100000; // 100 KB/s

    QCOMPARE(info.estimatedTimeRemaining(), 5); // 5 seconds
}

void TestEngine::testDownloadStatsFormatSpeed()
{
    DownloadStats stats;

    stats.speed = 512;
    QCOMPARE(stats.formattedSpeed(), QString("512 B/s"));

    stats.speed = 1024;
    QCOMPARE(stats.formattedSpeed(), QString("1.0 KB/s"));

    stats.speed = 1024 * 1024;
    QCOMPARE(stats.formattedSpeed(), QString("1.00 MB/s"));

    stats.speed = 1024 * 1024 * 1024;
    QCOMPARE(stats.formattedSpeed(), QString("1.00 GB/s"));
}

void TestEngine::testDownloadStatsFormatEta()
{
    DownloadStats stats;

    stats.eta = 30;
    QCOMPARE(stats.formattedEta(), QString("30s"));

    stats.eta = 90;
    QCOMPARE(stats.formattedEta(), QString("1m 30s"));

    stats.eta = 3661;
    QCOMPARE(stats.formattedEta(), QString("1h 1m"));

    stats.eta = -1;
    QCOMPARE(stats.formattedEta(), QString("Unknown"));
}

QTEST_MAIN(TestEngine)
#include "test_engine.moc"
