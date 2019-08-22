#ifndef WH1602_H
#define WH1602_H

//----------------------------------------------------------------------------------------------------
//������������ ����������
//----------------------------------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>

//----------------------------------------------------------------------------------------------------
//��������� �������
//----------------------------------------------------------------------------------------------------

void WH1602_Init(void);//������������� �������
void WH1602_SetTextUpLine(char *text);//�������� ����� � ������� ������� ������
void WH1602_SetTextDownLine(char *text);//�������� ����� � ������ ������� ������
void WH1602_SetTextProgmemUpLine(const char *text);//�������� ����� �� ������ � ������� ������� ������
void WH1602_SetTextProgmemDownLine(const char *text);//�������� ����� �� ������ � ������ ������� ������

#endif