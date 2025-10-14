#include "unfoldSpec.h"

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>
#include <QFile>
#include <QTextStream>

//用于高能滤片堆栈谱仪反解能谱

UnfoldSpec::UnfoldSpec() {}

UnfoldSpec::~UnfoldSpec() {}

// 滑动平均滤波函数
void smooth(double* data, double* output, int data_size, int window_size) {
    // 如果窗口大小大于数据长度，则不进行滤波
    if (window_size > data_size) {
        printf("窗口大小不能大于数据大小。\n");
        return;
    }

    // 窗口宽度必须是奇数
    if (window_size % 2 == 0) {
        printf("smooth,窗口宽度只能是奇数，当前窗口宽度%d。\n", window_size);
        return;
    }

    int halfWindow = (window_size - 1) / 2;

    // 计算每个点的滑动平均值
    for (int i = 0; i < data_size; i++) {
        double sum = 0.0;
        int count = 0;

        // 计算窗口内的平均值
        for (int j = i - (window_size - 1) / 2; j <= i + (window_size - 1) / 2; j++) {
            // 确保索引在有效范围内
            if (j >= 0 && j < data_size) {
                sum += data[j];
                count++;
            }
        }

        // 计算并存储当前位置的滑动平均值
        output[i] = sum / count;
    }

    // 重新处理左边界情况
    for (int i = 0; i < halfWindow; i++) {
        double sum = 0.0;
        int count = 0;
        //给出左侧端点个数
        int leftPoint = i;
        for (int j = 0; j <= i + i; j++) {
            // printf("Left side, i = %d, data[%d]=%f\n", i, j, data[j]);
            sum += data[j];
            count++;
        }
        output[i] = sum / count;
    }

    // 重新处理右边界情况
    for (int i = data_size - halfWindow; i < data_size; i++) {
        double sum = 0.0;
        int count = 0;
        //给出右侧端点个数
        int leftPoint = data_size - i - 1;
        for (int j = i - leftPoint; j < data_size; j++) {
            // printf("Right side, i = %d, data[%d]=%f\n", i, j, data[j]);
            sum += data[j];
            count++;
        }
        output[i] = sum / count;
    }
}

void UnfoldSpec::setWaveData(unsigned char* raw_data)
{
    ::memcpy(data, raw_data, PACK_BYTE);
    int stop_cell_DRS[2];
    unsigned int packages_data[SAMPLE_LENGTH][PACK_NUM];

    //将数据分解为32个包
    int pos_pack = 0;
    for (int i = 0; i < 32; i++)
    {
        if (i == 0) {
            stop_cell_DRS[0] = data[4] * 256 + data[5];
            stop_cell_DRS[1] = data[6] * 256 + data[7];
        }

        //跳过包前面部分8个字节
        pos_pack += 8;
        for (int j = 0; j < 1024; j++) {
            packages_data[j][i] = data[pos_pack++];
        }
    }

    //将采样点数据提取出来
    int dec_data[512][32];
    for (int i = 0; i < 32; i++)
    {
        for (int j = 0; j < 512; j++)
        {
            dec_data[j][i] = packages_data[j * 2][i] * 256 + packages_data[j * 2 + 1][i];
        }
    }

    //每相邻两个数据包合并到一个波形之中
    unsigned int native_data[1024][16];
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 512; j++)
        {
            native_data[j][i] = dec_data[j][i * 2];
            native_data[j + 512][i] = dec_data[j][i * 2 + 1];
        }
    }

    // int wave[1024][16];
    for (int i = 0; i < 16; i++)
    {
        if (i < 8)
        {
            int pos = 14 - 2 * i;
            for (int j = 0; j < 1024; j++)
            {
                wave[j][i] = native_data[j][pos];
            }
        }
        else
        {
            int pos = 31 - 2 * i;
            for (int j = 0; j < 1024; j++)
            {
                wave[j][i] = native_data[j][pos];
            }
        }
    }
}

#include <math.h>
bool UnfoldSpec::func_waveCorrect(unsigned char* raw_data)
{
    //loadData();
    ::memcpy(data, raw_data, PACK_BYTE);  
    loadCorrectData();

    int stop_cell_DRS[2];
    unsigned int packages_data[SAMPLE_LENGTH][PACK_NUM];

    //将数据分解为32个包
    int pos_pack = 0;
    for (int i = 0; i < 32; i++)
    {
        if (i == 0) {
            stop_cell_DRS[0] = data[4] * 256 + data[5];
            stop_cell_DRS[1] = data[6] * 256 + data[7];
        }

        //跳过包前面部分8个字节
        pos_pack += 8;
        for (int j = 0; j < 1024; j++) {
            packages_data[j][i] = data[pos_pack++];
        }
    }

    //将采样点数据提取出来
    unsigned int dec_data[512][32];
    for (int i = 0; i < 32; i++)
    {
        for (int j = 0; j < 512; j++)
        {
            dec_data[j][i] = packages_data[j * 2][i] * 256 + packages_data[j * 2 + 1][i];
        }
    }

    //每相邻两个数据包合并到一个波形之中
    unsigned int native_data[1024][16];
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 512; j++)
        {
            native_data[j][i] = dec_data[j][i * 2];
            native_data[j + 512][i] = dec_data[j][i * 2 + 1];
        }
    }

    // stop_cell_DRS[0] = stop_cell_DRS[0]+1;
    // stop_cell_DRS[1] = stop_cell_DRS[1]+1;

    double corr_wave[1024][16];// = {0.0};
    for (int i = 0; i < 16; i++)
    {
        if (i % 2 == 0)
        {
            int pos = stop_cell_DRS[0] + 1;
            for (int j = 0; j < 1024; j++)
            {
                int pos_id = pos % 1024;
                // std::cout << "pos_id = " << pos_id << std::endl;
                corr_wave[j][i] = corr_data[pos_id][i];
                pos++;
            }
        }
        else
        {
            int pos = stop_cell_DRS[1] + 1;
            for (int j = 0; j < 1024; j++)
            {
                int pos_id = pos % 1024;
                corr_wave[j][i] = corr_data[pos_id][i];
                pos++;
            }
        }
    }

    // int wave[1024][16];
    for (int i = 0; i < 16; i++)
    {
        if (i < 8)
        {
            int pos = 14 - 2 * i;
            for (int j = 0; j < 1024; j++)
            {
                wave[j][i] = (std::max)((int)(native_data[j][pos] - round(corr_wave[j][pos]) + 8192), (int)0);
                //wave[j][i] = native_data[j][pos] - round(corr_wave[j][pos]) + 8192;
            }
        }
        else
        {
            int pos = 31 - 2 * i;
            for (int j = 0; j < 1024; j++)
            {
                wave[j][i] = (std::max)((int)(native_data[j][pos] - round(corr_wave[j][pos]) + 8192), (int)0);
                //wave[j][i] = native_data[j][pos] - round(corr_wave[j][pos]) + 8192;
            }
        }
    }

    return true;
}

QMap<quint8, QVector<quint16>> UnfoldSpec::getCorrWaveData() const
{
    //获取矫正后波形数据
    
    //颠倒wave各通道顺序 0-7颠倒 8-15颠倒
    //unsigned int wave_temp[1024][16];
    //for (int i = 0; i < 16; ++i) {
    //    for (int j = 0; j < SAMPLE_LENGTH; ++j) {
    //        if (i < 8)
    //        {
    //            wave_temp[j][i] = wave[j][7 - i];
    //        }
    //        else {
    //            wave_temp[j][i] = wave[j][23 - i];
    //        }
    //    }
    //}

    QMap<quint8, QVector<quint16>> result;
    for (int i = 0; i <= 15; ++i) {
        QVector<quint16> waveData;
        for (int j = 0; j < 1024; ++j) {
            //waveData.push_back(wave_temp[j][i]);
            waveData.push_back(wave[j][i]);
        }

        result[i] = waveData;
    }

    return result;
}

QVector<QPair<double, double>> UnfoldSpec::getUnfoldWaveData() const
{
    //获取反解后波形数据
    QVector<QPair<double, double>> result;
    for (int j = 0; j < energyPoint; ++j)
    {
        if (spec[0][j] >= 0.05 && spec[0][j] <= 3.0)
            result.push_back(qMakePair<double,double>(spec[0][j], spec[1][j]));
    }

    return result;
}

bool UnfoldSpec::unfold()
{
    readResMatrix(responceFileName);
    loadSeq();

    for (int i = 0; i < DET_NUM; ++i) {
        for (int j = 0; j < energyPoint; ++j) {
            if (responce_matrix[i][j] < 1.0e-9)
                responce_matrix[i][j] = 1.0e-9;
        }
    }

    //颠倒wave各通道顺序 0-7颠倒 8-15颠倒
    unsigned int wave_temp[1024][16];
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < SAMPLE_LENGTH; ++j) {
            if (i < 8)
            {
                wave_temp[j][i] = wave[j][7 - i];                
            }
            else {
                wave_temp[j][i] = wave[j][23 - i];
            }
        }
    }

    // 限制响应矩阵的最小值
    double Threshold = 1e-3;
    double phi_0[energyPoint] = { 1.0 };

    double data_new[DET_NUM][SAMPLE_LENGTH];
    double pulseMean[DET_NUM]; //各通道脉冲幅度均值
    for (int i = 0; i < DET_NUM; ++i) {
        //求基线均值，取前n个点求平均
        double total = 0.0;
        int baseNum = 10;
        for (int j = 0; j < SAMPLE_LENGTH; ++j) {
            data_new[i][j] = wave_temp[j][i];
            if (j < baseNum) total += wave_temp[j][i];
        }

        pulseMean[i] = total / baseNum;
    }

    //扣基线
    for (int i = 0; i < DET_NUM; ++i) {
        for (int j = 0; j < SAMPLE_LENGTH; ++j) {
            //std::cout << "data_new[" << i << "][" << j << "] = " << data_new[i][j] << ", pulseMean[" << i << "]=" << pulseMean[i] << std::endl;
            //data_new[i][j] = data_new[i][j] - pulseMean[i];// 防止0出现
            data_new[i][j] = (std::max)((int)(data_new[i][j] - pulseMean[i]), (int)0);
        }
    }

    // 求和
    double data_pulse[DET_NUM] = { 0.0 };
    for (int i = 0; i < DET_NUM; ++i) {
        for (int j = 0; j < SAMPLE_LENGTH; ++j) {
            data_pulse[i] += data_new[i][j];
        }
        data_pulse[i] = data_pulse[i] * sampleTime * 8.729797541536420e+09 + 1.255583997462927e+04;
    }

    double rou_old[DET_NUM];
    for (int i = 0; i < DET_NUM; ++i)
    {
        rou_old[i] = 1.0 / sqrt(data_pulse[i]);
    }

    double log_phi_new[energyPoint] = { 0.0 };
    double log_phi_old[energyPoint] = { 0.0 };
    double N_old[DET_NUM] = { 0.0 };
    for (int i = 0; i < DET_NUM; ++i) {
        double total = 0.0;
        for (int j = 0; j < energyPoint; ++j) {
            double value = responce_matrix[i][j] * exp(log_phi_old[j]);
            total += value;
            // std::cout << "responce_matrix[" << i << "][" << j << "] = " << responce_matrix[i][j]
            //         << ", value =" << value << ", total = " << total << std::endl;
            N_old[i] += responce_matrix[i][j] * exp(log_phi_old[j]);
        }
    }

    double log_N_old[DET_NUM];
    for (int i = 0; i < DET_NUM; ++i) {
        log_N_old[i] = log(N_old[i]);
    }

    double omega_old[DET_NUM][energyPoint];
    for (int j = 0; j < energyPoint; ++j) {
        for (int i = 0; i < DET_NUM; ++i) {
            omega_old[i][j] = responce_matrix[i][j] * exp(log_phi_old[j]) / N_old[i];
        }
    }

    double lambda_old[energyPoint] = { 0.0 };
    for (int j = 0; j < energyPoint; ++j) {
        for (int i = 0; i < DET_NUM; ++i) {
            lambda_old[j] += omega_old[i][j] / rou_old[i];
        }
        lambda_old[j] = 1.0 / lambda_old[j];
    }

    double Chi = 0.0;
    for (int i = 0; i < DET_NUM; ++i)
    {
        Chi += pow(log(data_pulse[i]) - log_N_old[i], 2) / rou_old[i];
    }

    int count = 1;

    double Chi_old;
    while (1)
    {
        Chi_old = Chi;
        for (int j = 0; j < energyPoint; ++j)
        {
            double sum = 0.0;
            for (int i = 0; i < DET_NUM; ++i)
            {
                sum += (log(data_pulse[i]) - log_N_old[i]) / rou_old[i] * omega_old[i][j];
            }
            log_phi_new[j] = log_phi_old[j] + lambda_old[j] * sum;
        }
        smooth(log_phi_new, log_phi_old, energyPoint, 5);

        for (int i = 0; i < DET_NUM; ++i)
        {
            double sum = 0.0;
            for (int j = 0; j < energyPoint; ++j)
            {
                sum += responce_matrix[i][j] * exp(log_phi_old[j]);
            }
            N_old[i] = sum;
        }

        for (int i = 0; i < DET_NUM; ++i)
        {
            log_N_old[i] = log(N_old[i]);
        }

        for (int i = 0; i < DET_NUM; ++i)
        {
            double sum = 0.0;
            for (int j = 0; j < energyPoint; ++j)
            {
                omega_old[i][j] = responce_matrix[i][j] * exp(log_phi_old[j]) / N_old[i];
                sum += omega_old[i][j] / rou_old[i];
            }
        }

        for (int j = 0; j < energyPoint; ++j)
        {
            double sum = 0.0;
            for (int i = 0; i < DET_NUM; ++i)
            {
                sum += omega_old[i][j] / rou_old[i];
            }
            lambda_old[j] = 1.0 / sum;
        }

        Chi = 0.0;
        for (int i = 0; i < DET_NUM; ++i)
        {
            Chi += pow(log(data_pulse[i]) - log_N_old[i], 2) / rou_old[i];
        }

        count++;

        if ((abs(Chi - Chi_old) / Chi) < Threshold) break;
        if (count > 100)  break;
    }

    // double spec[energyPoint];
    for (int j = 0; j < energyPoint; ++j)
    {
        spec[0][j] = seq_energy[j];
        spec[1][j] = exp(log_phi_old[j]);
        //std::cout << " spec[" << j << "] =" << spec[0][j] << " " << spec[1][j] << std::endl;
    }

    return true;
}

//读取波形采用数据
bool UnfoldSpec::loadData()
{
    std::string fileName = "./data.csv";
    std::ifstream file(fileName);
    if (!file.is_open())
    {
        std::cout << "Cannot open file:" << fileName << std::endl;
        return false;
    }

    int count = 0;
    std::string line;
    while (std::getline(file, line) && count < PACK_BYTE)
    {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        unsigned int value = std::stoul(line);
        data[count] = value;
        count++;
    }

    file.close();
    return true;
}

//读取波形修正数据
bool UnfoldSpec::loadCorrectData()
{
    std::string fileName = "./rom_wave_corr.csv";
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cout << "Cannot open file:" << fileName << std::endl;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line) && lineNumber < SAMPLE_LENGTH) {
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 分割CSV行
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string item;

        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                parts.push_back(item);
            }
        }

        // 如果逗号分隔不够，尝试分号分隔
        if (parts.size() < CHANNEL_NUM) {
            parts.clear();
            std::stringstream ss2(line);
            while (std::getline(ss2, item, ';')) {
                if (!item.empty()) {
                    parts.push_back(item);
                }
            }
        }

        if (parts.size() < CHANNEL_NUM) {
            std::cout << "line " << lineNumber << ", column number is not enough, skip this line" << std::endl;
            continue;
        }

        // 转换为double
        for (int i = 0; i < CHANNEL_NUM && i < parts.size(); i++) {
            try {
                double value = std::stod(parts[i]);
                corr_data[lineNumber][i] = value;
            }
            catch (const std::exception& e) {
                std::cout << "line " << lineNumber << ", fail in data type transform: " << line << std::endl;
                file.close();
                return false;
            }
        }

        lineNumber++;
    }

    file.close();
    return true;
}

bool UnfoldSpec::loadSeq()
{
    QFile file("./seq_energy.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int count = 0;
    QTextStream stream(&file);
    while (!stream.atEnd() && count<energyPoint)
    {
        QString line = stream.readLine();
        double value = line.toDouble();

        seq_energy[count] = value;
        // qDebug()<<"count = "<<count<<", "<<value<<", seq_energy[i]="<<seq_energy[count];
        count++;
    }

    file.close();
    return true;
}

/**
 * @brief readResMatrix
 * 读取相应矩阵
 */
bool UnfoldSpec::readResMatrix(std::string fileName)
{
    fileName = "./responce_matrix.csv";
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cout << "无法打开文件:" << fileName << std::endl;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line) && lineNumber < DET_NUM) {
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 分割CSV行
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string item;

        while (std::getline(ss, item, ',')) {
            if (!item.empty()) {
                parts.push_back(item);
            }
        }

        // 如果逗号分隔不够，尝试分号分隔
        if (parts.size() < energyPoint) {
            parts.clear();
            std::stringstream ss2(line);
            while (std::getline(ss2, item, ';')) {
                if (!item.empty()) {
                    parts.push_back(item);
                }
            }
        }

        if (parts.size() < energyPoint) {
            std::cout << "line " << lineNumber << ", columns is not enough, skip this line!" << std::endl;
            continue;
        }

        // 转换为double
        for (int i = 0; i < energyPoint && i < parts.size(); i++) {
            try {
                double value = std::stod(parts[i]);
                responce_matrix[lineNumber][i] = value;
            }
            catch (const std::exception& e) {
                std::cout << "line " << lineNumber << ", fail in data type transform: " << line << std::endl;
                file.close();
                return false;
            }
        }

        lineNumber++;
    }

    file.close();
    return true;
}
