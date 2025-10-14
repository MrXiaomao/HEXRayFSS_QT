#pragma once

// unfoldspec.h
#ifndef UNFOLDSPEC_H
#define UNFOLDSPEC_H

#include <QMap>
#include <QVector>

// #define PACK_BYTE (1024+8)*32 //数据包大小

//每次触发事件，FPGA上传16通道波形数据，只含有一个波形。
//最后一个通道数据丢弃不要，只处理15通道的数据，每个通道采样点1024个，每个采样点2个字节。共分为32个包发送出来。
//单次采样事件数据帧长度为（8+1024）×32=33024

const int PACK_BYTE = 33024;
const int PACK_NUM = 32; //数据包个数
const int SAMPLE_LENGTH = 1024; //采样点数
const int CHANNEL_NUM = 16; //FPGA 采用通道
const int DET_NUM = 15; //探测器个数，有一个探测器通道未使用

const int energyPoint = 500; //反解函数中能谱的道数

class UnfoldSpec
{
public:
    UnfoldSpec();
    ~UnfoldSpec();

public:
    const int waveNum = 15;
    void setWaveData(unsigned char* raw_data);
    bool func_waveCorrect(unsigned char* raw_data);
    bool unfold();
    void setResFileName(std::string name) {
        responceFileName = name;
    }

    QMap<quint8, QVector<quint16>> getCorrWaveData() const;    //获取矫正后波形数据
    QVector<QPair<double, double>> getUnfoldWaveData() const;//获取反解后波形数据

private:
    bool loadData();
    bool loadCorrectData();
    bool loadSeq(); //读取能量点数据
    bool readResMatrix(std::string fileName); //读取响应矩阵

    unsigned char data[PACK_BYTE]; //网口原始数据，按单个字节取出。
    unsigned int wave[1024][16]; //波形数据
    double corr_data[SAMPLE_LENGTH][CHANNEL_NUM];

    double seq_energy[energyPoint];
    double responce_matrix[DET_NUM][energyPoint];
    double sampleTime = 1e-8; //两个采样点的间隔时间，单位s
    double spec[2][energyPoint];
    std::string responceFileName;
};

#endif // UNFOLDSPEC_H

