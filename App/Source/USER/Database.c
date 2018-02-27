/************************************************************************************************
*                                   SRWF-6009
*    (c) Copyright 2015, Software Department, Sunray Technology Co.Ltd
*                               All Rights Reserved
*
* FileName     : Database.c
* Description  :
* Version      :
* Function List:
*------------------------------Revision History--------------------------------------------------
* No.   Version     Date            Revised By      Item            Description
* 1     V1.1        08/11/2015      Zhangxp         SRWF-6009       Original Version
************************************************************************************************/

#define DATABASE_GLOBALS

/************************************************************************************************
*                             Include File Section
************************************************************************************************/
#include "Stm32f10x_conf.h"
#include "ucos_ii.h"
#include "Bsp.h"
#include "Main.h"
#include "Rtc.h"
#include "Gprs.h"
#include "Eeprom.h"
#include "Flash.h"
#include "Led.h"
#include "SerialPort.h"
#include "DataHandle.h"
#include "Database.h"
#include <string.h>

/************************************************************************************************
*                        Global Variable Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Prototype Declare Section
************************************************************************************************/

/************************************************************************************************
*                           Function Declare Section
************************************************************************************************/

/************************************************************************************************
* Function Name: Data_Init
* Decription   : ���ݿ��ʼ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Data_Init(void)
{
    Flash_LoadConcentratorInfo();
    Flash_LoadSubNodesInfo();
    Data_RefreshNodeStatus();
}

/************************************************************************************************
* Function Name: Data_RefreshNodeStatus
* Decription   : ����ˢ��,��Eeprom�ж������ݲ�ˢ�½ڵ��״̬
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Data_RefreshNodeStatus(void)
{
    uint8 meterDataLen;
    uint16 nodeId;
    METER_DATA_SAVE_FORMAT *meterBufPtr;

    if ((void *)0 == (meterBufPtr = OSMemGetOpt(SmallMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return;
    }
    for (nodeId = 0; nodeId < Concentrator.MaxNodeId; nodeId++) {
        if (0 == memcmp(SubNodes[nodeId].LongAddr, NullAddress, LONG_ADDR_SIZE)) {
            continue;
        }
        // �ж��Ƿ��Ǽ�����,�������ų�
        if ((SubNodes[nodeId].DevType & 0xF0) == 0xF0) {
            continue;
        }
        // ��ȡEeprom�ж�Ӧ��ֵ
        meterDataLen = sizeof(METER_DATA_SAVE_FORMAT);
        Eeprom_ReadWrite((uint8 *)meterBufPtr, nodeId * NODE_INFO_SIZE, meterDataLen, READ_ATTR);
        if (0 == memcmp(meterBufPtr->Address, SubNodes[nodeId].LongAddr, LONG_ADDR_SIZE)) {
            SubNodes[nodeId].Property.LastResult = meterBufPtr->Property.LastResult;
            SubNodes[nodeId].Property.CurRouteNo = meterBufPtr->Property.CurRouteNo;
            SubNodes[nodeId].Property.UploadData = meterBufPtr->Property.UploadData;
            SubNodes[nodeId].Property.UploadPrio = meterBufPtr->Property.UploadPrio;
            SubNodes[nodeId].CmdProp.Content = meterBufPtr->CmdProp.Content;
            memcpy(&SubNodes[nodeId].CmdData, &meterBufPtr->CmdData, sizeof(CMD_DATA));
            SubNodes[nodeId].RxChannel = meterBufPtr->RxChannel;
            SubNodes[nodeId].TxChannel = meterBufPtr->TxChannel;
            SubNodes[nodeId].AutoChannelSwitch = meterBufPtr->AutoChannelSwitch;
            SubNodes[nodeId].RxMeterVersion = meterBufPtr->RxMeterVersion;
        } else {
            SubNodes[nodeId].Property.LastResult = 2;
            SubNodes[nodeId].Property.UploadData = TRUE;
            SubNodes[nodeId].Property.UploadPrio = FALSE;
            SubNodes[nodeId].CmdProp.Content = 0x0;
            memset(&SubNodes[nodeId].CmdData, 0, sizeof(CMD_DATA));
            SubNodes[nodeId].RxChannel = DEFAULT_TX_CHANNEL;
            SubNodes[nodeId].TxChannel = DEFAULT_RX_CHANNEL;
            SubNodes[nodeId].AutoChannelSwitch = FALSE;
            SubNodes[nodeId].RxMeterVersion = 0;
        }
    }
    OSMemPut(SmallMemoryPtr, meterBufPtr);
}


/************************************************************************************************
* Function Name: Data_MeterDataInit
* Decription   : �����ݳ�ʼ��
* Input        : MeterBufPtr-���ݴ洢��ָ��,NodeId-����,MeterDataLen-��ʼ������ĳ���
* Output       : ��
* Others       : ��
************************************************************************************************/
void Data_MeterDataInit(METER_DATA_SAVE_FORMAT *MeterBufPtr, uint16 NodeId, uint8 MeterDataLen)
{
    memset(MeterBufPtr, 0, MeterDataLen);
    memcpy(MeterBufPtr->Address, SubNodes[NodeId].LongAddr, LONG_ADDR_SIZE);
    MeterBufPtr->Property.UploadData= SubNodes[NodeId].Property.UploadData = TRUE;
    MeterBufPtr->Property.UploadPrio = SubNodes[NodeId].Property.UploadPrio = FALSE;
	SubNodes[NodeId].CmdProp.Content = 0x0;
	memset(&SubNodes[NodeId].CmdData, 0, sizeof(CMD_DATA));
    MeterBufPtr->RxChannel = SubNodes[NodeId].RxChannel = DEFAULT_TX_CHANNEL;
    MeterBufPtr->TxChannel = SubNodes[NodeId].TxChannel = DEFAULT_RX_CHANNEL;
    MeterBufPtr->AutoChannelSwitch = SubNodes[NodeId].AutoChannelSwitch = FALSE;
    MeterBufPtr->RxMeterVersion = SubNodes[NodeId].RxMeterVersion = 0;
}

/************************************************************************************************
* Function Name: Data_SetConcentratorAddr
* Decription   : ���ü�������ַ,ֻ��ͨ���������ӵ�ͨ���趨
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:������ID��BCD��(6)
*                ����:����״̬(1)
************************************************************************************************/
void Data_SetConcentratorAddr(DATA_FRAME_STRUCT *DataFrmPtr)
{
    if (DataFrmPtr->PortNo != Usart_Debug && DataFrmPtr->PortNo != Usb_Port) {
        DataFrmPtr->DataBuf[0] = OP_NoFunction;
    } else {
        if (0 == memcmp(Concentrator.LongAddr, DataFrmPtr->DataBuf, LONG_ADDR_SIZE)) {
            DataFrmPtr->DataBuf[0] = OP_Succeed;
        } else if (NULL_U16_ID != Data_FindNodeId(0, DataFrmPtr->DataBuf)) {
            DataFrmPtr->DataBuf[0] = OP_ObjectRepetitive;
        } else {
            memcpy(Concentrator.LongAddr, DataFrmPtr->DataBuf, LONG_ADDR_SIZE);
            Flash_SaveConcentratorInfo();
            DataFrmPtr->DataBuf[0] = OP_Succeed;
            DevResetTimer = 10000;      // 10���Ӻ��豸��λ
        }
    }
    DataFrmPtr->DataLen = 1;
    return;
}

/************************************************************************************************
* Function Name: Data_FindNodeId
* Decription   : ��ָ��λ�ò���ָ�����ԵĽڵ�ID
* Input        : StartId-ָ�����ҵĿ�ʼλ��, BufPtr-ָ��ڵ㳤��ַ��ָ��
* Output       : ����Ҫ��Ľڵ�IDֵ
* Others       : ��
************************************************************************************************/
uint16 Data_FindNodeId(uint16 StartId, uint8 *BufPtr)
{
    uint16 i;

    if (0 == memcmp(BufPtr, Concentrator.LongAddr, LONG_ADDR_SIZE)) {
        return DATA_CENTER_ID;
    }
    if (0 == memcmp(BufPtr, NullAddress, LONG_ADDR_SIZE)) {
        return NULL_U16_ID;
    }
    for (i = StartId; i < Concentrator.MaxNodeId; i++) {
        if (0 == memcmp(BufPtr, SubNodes[i].LongAddr, LONG_ADDR_SIZE)) {
            return i;
        }
    }
    return NULL_U16_ID;
}

/************************************************************************************************
* Function Name: Data_ClearMeterData
* Decription   : ������б�����,���ӽڵ��ϴ�������
* Input        : ��
* Output       : �ɹ����ߴ���
* Others       : ��Ҫ�Ǳ�����Eeprom�е�����
************************************************************************************************/
bool Data_ClearMeterData(void)
{
    uint8 *msg;
    uint16 nodeId;

    if ((void *)0 == (msg = OSMemGetOpt(SmallMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        return ERROR;
    }

    memset(msg, 0xFF, LONG_ADDR_SIZE);
    for (nodeId = 0; nodeId < MAX_NODE_NUM; nodeId++) {
        Eeprom_ReadWrite(msg, nodeId * NODE_INFO_SIZE, LONG_ADDR_SIZE, WRITE_ATTR);
    }
    OSMemPut(SmallMemoryPtr, msg);
    return SUCCESS;
}

/************************************************************************************************
* Function Name: Data_ClearDatabase
* Decription   : ���ݿ��ʼ��,������б�����,�м�����,�Զ���·��,������Ϣ��
* Input        : ��
* Output       : ��
* Others       : ��
************************************************************************************************/
void Data_ClearDatabase(void)
{
    if (0 == Concentrator.MaxNodeId) {
        return;
    }
    Concentrator.MaxNodeId = 0;
    Flash_SaveConcentratorInfo();
    memset((uint8 *)(&SubNodes), 0xFF, sizeof(SUBNODE_INFO) * MAX_NODE_NUM);
    OSSchedLock();
    Flash_Erase(FLASH_NODE_INFO_ADDRESS, FLASH_NODE_INFO_PAGE_SIZE);
    Flash_Erase(FLASH_CUSTOM_ROUTE_ADDRESS1, FLASH_CUSTOM_ROUTE_PAGE_SIZE);
    Flash_Erase(FLASH_CUSTOM_ROUTE_ADDRESS2, FLASH_CUSTOM_ROUTE_PAGE_SIZE);
    OSSchedUnlock();
}

/************************************************************************************************
* Function Name: Data_ReadNodesCount
* Decription   : ���ݿ���ڵ������
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:�豸����(1)
*                ����:�豸����(1)+��������(2)
************************************************************************************************/
void Data_ReadNodesCount(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 *dataBufPtr = DataFrmPtr->DataBuf;
    uint16 i, nodeCount;

    nodeCount = 0;
    for (i = 0; i < Concentrator.MaxNodeId; i++) {
        if (0 == memcmp(SubNodes[i].LongAddr, NullAddress, LONG_ADDR_SIZE)) {
            continue;
        }
        if (0xFF == *dataBufPtr || *dataBufPtr == SubNodes[i].DevType) {
            nodeCount++;
        }
    }
    *(dataBufPtr + 1) = (uint8)nodeCount;
    *(dataBufPtr + 2) = (uint8)(nodeCount >>8);
    DataFrmPtr->DataLen = 3;
}

/************************************************************************************************
* Function Name: Data_ReadNodes
* Decription   : ���ݿ���ڵ���Ϣ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:�豸����(1)+��ʼ���(2)+��ȡ������
*                ����:���豸���ͽڵ�������(2)+����Ӧ�������+N����ߵ���Ϣ(8N)
************************************************************************************************/
void Data_ReadNodes(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 devType, rdCount, ackCount, *p, *dataBufPtr;
    uint16 i, rdStart, ackTotal;

    dataBufPtr = DataFrmPtr->DataBuf;
    devType = *dataBufPtr;
    rdStart = *(dataBufPtr + 1) + *(dataBufPtr + 2) * 256;
    rdCount = *(dataBufPtr + 3);

    ackTotal = 0;
    ackCount = 0;
    p = dataBufPtr + 3;
    for (i = 0; i < Concentrator.MaxNodeId; i++) {
        if (0 == memcmp(SubNodes[i].LongAddr, NullAddress, LONG_ADDR_SIZE)) {
            continue;
        }
        if (0xFF == devType || SubNodes[i].DevType == devType) {
            if (ackTotal >= rdStart && ackCount < rdCount) {
                memcpy(p, SubNodes[i].LongAddr, LONG_ADDR_SIZE);
                p += LONG_ADDR_SIZE;
                *p++ = SubNodes[i].DevType;
                *p++ = SubNodes[i].Property.LastResult;
                ackCount++;
            }
            ackTotal++;
        }
    }
    *dataBufPtr = (uint8)ackTotal;
    *(dataBufPtr + 1) = (uint8)(ackTotal >> 8);
    *(dataBufPtr + 2) = ackCount;
    DataFrmPtr->DataLen = ackCount * (LONG_ADDR_SIZE + 2) + 3;
}

/************************************************************************************************
* Function Name: Data_WriteNodes
* Decription   : ���ݿ�д�ڵ���Ϣ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:���ýڵ�����+���һ����ʶ+N����ߵ�BCD��(8N:������ַ,�豸����,���һ�γ�����)
*                ����:���ýڵ�����+��ߵ�BCD��Ͳ���״̬��(7N)
************************************************************************************************/
void Data_WriteNodes(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 err, nodeNum, saveNow, *rdPtr, *wrPtr, *dataBufPtr;
    uint16 i;
    bool save = FALSE;
    OP_RESULT result;

    // ��ȡ���ýڵ�����������һ����ʶ
    dataBufPtr = DataFrmPtr->DataBuf;
    nodeNum = *dataBufPtr;
    saveNow = *(dataBufPtr + 1);
    if (DataFrmPtr->DataLen != nodeNum * (LONG_ADDR_SIZE + 2) + 2) {
        *dataBufPtr = OP_FormatError;
        DataFrmPtr->DataLen = 1;
        return;
    }
    // ���ö�дָ��
    rdPtr = dataBufPtr + 2;
    wrPtr = dataBufPtr + 1;
    // ��ڵ��б������ӽڵ�
    while (nodeNum-- > 0) {
        if (NULL_U16_ID != Data_FindNodeId(0, rdPtr)) {
            result = OP_ObjectRepetitive;
        } else {
            if (Concentrator.MaxNodeId >= MAX_NODE_NUM) {
                result = OP_Objectoverflow;
            } else {
                save = TRUE;
                // Ѱ�ҿ�λ��
                for (i = 0; i < Concentrator.MaxNodeId; i++) {
                    if (0 == memcmp(SubNodes[i].LongAddr, NullAddress, LONG_ADDR_SIZE)) {
                        break;
                    }
                }
                if (i >= Concentrator.MaxNodeId) {
                    Concentrator.MaxNodeId++;
                }
                memcpy(SubNodes[i].LongAddr, rdPtr, LONG_ADDR_SIZE);
                SubNodes[i].DevType = (DEVICE_TYPE)(*(rdPtr + LONG_ADDR_SIZE));
                SubNodes[i].Property.LastResult = 2;
                SubNodes[i].Property.UploadData = TRUE;
                SubNodes[i].Property.UploadPrio = LOW;
				SubNodes[i].CmdProp.Content = 0x0;
				memset(&SubNodes[i].CmdData, 0, sizeof(CMD_DATA));
                SubNodes[i].RxChannel = DEFAULT_TX_CHANNEL;
                SubNodes[i].TxChannel = DEFAULT_RX_CHANNEL;
                SubNodes[i].AutoChannelSwitch = FALSE;
                SubNodes[i].RxMeterVersion = 0;
                result = OP_Succeed;
            }
        }
        memcpy(wrPtr, rdPtr, LONG_ADDR_SIZE);
        *(wrPtr + LONG_ADDR_SIZE) = result;
        rdPtr += LONG_ADDR_SIZE + 2;
        wrPtr += LONG_ADDR_SIZE + 1;
    }

    // ��ʱ�󱣴�,������ʱͬ����ʱ��,������������ݲ�һ�µ����
    if (TRUE == save) {
        if (saveNow) {
            OSFlagPost(GlobalEventFlag, (OS_FLAGS)FLAG_DELAY_SAVE_TIMER, OS_FLAG_SET, &err);
        } else {
            SubNodesSaveDelayTimer = DATA_SAVE_DELAY_TIMER;
        }
    }
    DataFrmPtr->DataLen = 1 + (LONG_ADDR_SIZE + 1) * *DataFrmPtr->DataBuf;
}

/************************************************************************************************
* Function Name: Data_DeleteNodes
* Decription   : ���ݿ�ɾ���ڵ���Ϣ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:N����ߵ�BCD��(6N)
*                ����:��ߵ�BCD��Ͳ���״̬��(7N)
*          ע��: ɾ����Ҫ�������˽ڵ���Զ���·�ɺ��Դ˽ڵ�Ϊ�ھӵĽڵ�ɾ����
************************************************************************************************/
void Data_DeleteNodes(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 *dataBufPtr;
    uint16 i, j, nodeId, nodeNum;

    dataBufPtr = DataFrmPtr->DataBuf;
    nodeNum = DataFrmPtr->DataLen / LONG_ADDR_SIZE;
    // ��Ϊ���ص����ݱ�ԭ����Ҫ���ҹ���ͬһ���洢����,����Ҫ�������ݰ���
    i = nodeNum * LONG_ADDR_SIZE - 1;
    j = nodeNum * (LONG_ADDR_SIZE + 1) - 1;
    while (i > LONG_ADDR_SIZE - 2) {
        if (0 == (j + 1) % (LONG_ADDR_SIZE + 1)) {
            *(dataBufPtr + j) = OP_Executing;
            j -= 1;
        }
        *(dataBufPtr + j--) = *(dataBufPtr + i--);
    }

    // �ӽڵ��б���ɾ���ڵ�
    while (nodeNum-- > 0) {
        nodeId = Data_FindNodeId(0, dataBufPtr);
        if (NULL_U16_ID == nodeId || DATA_CENTER_ID == nodeId) {
            *(dataBufPtr + LONG_ADDR_SIZE) = OP_ObjectNotExist;
        } else {
            if (Concentrator.MaxNodeId > 0 && nodeId == Concentrator.MaxNodeId - 1) {
                Concentrator.MaxNodeId--;
            }
            memcpy(SubNodes[nodeId].LongAddr, NullAddress, LONG_ADDR_SIZE);
            *(dataBufPtr + LONG_ADDR_SIZE) = OP_Succeed;
        }
        dataBufPtr += LONG_ADDR_SIZE + 1;
    }

    // ���漯������Ϣ�ͽڵ���Ϣ
    Flash_SaveConcentratorInfo();
    Flash_SaveSubNodesInfo();

    nodeNum = DataFrmPtr->DataLen / LONG_ADDR_SIZE;
    DataFrmPtr->DataLen = (LONG_ADDR_SIZE + 1) * nodeNum;
}

/************************************************************************************************
* Function Name: Data_ModifyNodes
* Decription   : ���ݿ���Ľڵ��ַ��Ϣ(0x54)
* Input        : BufPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:ԭ���(6)+�±��(6)+�豸����
*                ����:����״̬
************************************************************************************************/
void Data_ModifyNodes(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint16 oldNodeId, newNodeId;
    uint8 *dataBufPtr = DataFrmPtr->DataBuf;

    DataFrmPtr->DataLen = 1;

    // ����ԭ����Ƿ����
    oldNodeId = Data_FindNodeId(0, dataBufPtr);
    if (NULL_U16_ID == oldNodeId) {
        *dataBufPtr = OP_ObjectNotExist;
        return;
    }
    // �����±���Ƿ����
    newNodeId = Data_FindNodeId(0, dataBufPtr + LONG_ADDR_SIZE);
    if (DATA_CENTER_ID == newNodeId) {
        *dataBufPtr = OP_ObjectRepetitive;
        return;
    }
    if (NULL_U16_ID != newNodeId && newNodeId != oldNodeId) {
        *dataBufPtr = OP_ObjectRepetitive;
        return;
    }
    // �滻ԭ��Ų�����
    memcpy(SubNodes[oldNodeId].LongAddr, dataBufPtr + LONG_ADDR_SIZE, LONG_ADDR_SIZE);
    SubNodes[oldNodeId].DevType = (DEVICE_TYPE)(*(dataBufPtr + LONG_ADDR_SIZE * 2));
    Flash_SaveSubNodesInfo();
    *dataBufPtr = OP_Succeed;
    return;
}

/************************************************************************************************
* Function Name: Data_GetTimeSlot
* Decription   : ��ȡĳ���ڵ��ʱ϶���ܵĽڵ���
* Input        : NodeIdPtr-�ڵ��ַָ��,TimeSlotPtr-ָ�򱣴�ýڵ�ʱ϶�ͽڵ�������ָ��
* Output       : None
* Others       : ʱ϶Ϊ4���ֽ�,����Ϊʱ϶���ֽ�,ʱ϶���ֽ�,�ܱ������ֽ�,�ܱ������ֽ�.
************************************************************************************************/
void Data_GetTimeSlot(uint8 *NodeIdPtr, uint8 *TimeSlotPtr)
{
    uint16 i, timeSlot, nodeTotal;

    timeSlot = 0;
    nodeTotal = 0;
    // Ѱ�Ҹýڵ��ʱ϶ֵ
    for (i = 0; i < Concentrator.MaxNodeId; i++) {
        if (0 == memcmp(SubNodes[i].LongAddr, NullAddress, LONG_ADDR_SIZE)) {
            continue;
        }
        nodeTotal++;
        if (0 == memcmp(SubNodes[i].LongAddr, NodeIdPtr, LONG_ADDR_SIZE)) {
            timeSlot = nodeTotal;
        }
    }
    if (0 == timeSlot) {
        memset(TimeSlotPtr, 0, 4);
    } else {
        *TimeSlotPtr++ = (uint8)timeSlot;
        *TimeSlotPtr++ = (uint8)(timeSlot >> 8);
        *TimeSlotPtr++ = (uint8)nodeTotal;
        *TimeSlotPtr++ = (uint8)(nodeTotal >> 8);
    }
}

/************************************************************************************************
* Function Name: Data_BatchWriteMeterCmdLoadProc
* Decription   : ����д����·�����洦����(0x69)
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����:(��ߺ�[6]+����[1+8])*N
*                ����:(��ߺ�[6]+����״̬[1])*N
************************************************************************************************/
void Data_BatchWriteMeterCmdLoadProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 *msg, *txDataBufPtr, *rxDataBufPtr;
    uint16 nodeId;
    METER_DATA_SAVE_FORMAT *meterBufPtr;
	uint8 meterDataLen;

    if ((void *)0 == (msg = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        DataFrmPtr->DataBuf[0] = OP_Failure;
        DataFrmPtr->DataLen = 1;
        return;
    }
    // ������һ���ڴ�,���ڶ�ȡEeprom�е�����
    if ((void *)0 == (meterBufPtr = OSMemGetOpt(SmallMemoryPtr, 10, TIME_DELAY_MS(50)))) {
        DataFrmPtr->DataBuf[0] = OP_Failure;
        DataFrmPtr->DataLen = 1;
        OSMemPut(LargeMemoryPtr, msg);
        return ;
    }
    meterDataLen = sizeof(METER_DATA_SAVE_FORMAT);

    memcpy(msg, DataFrmPtr->DataBuf, DataFrmPtr->DataLen);
    txDataBufPtr = DataFrmPtr->DataBuf;
    rxDataBufPtr = msg;
    while (rxDataBufPtr - msg < DataFrmPtr->DataLen) {
        nodeId = Data_FindNodeId(0, rxDataBufPtr);
        memcpy(txDataBufPtr, rxDataBufPtr, LONG_ADDR_SIZE);
        txDataBufPtr += LONG_ADDR_SIZE;
        rxDataBufPtr += LONG_ADDR_SIZE;
        if (NULL_U16_ID == nodeId) {
            *txDataBufPtr++ = OP_ObjectNotExist;
        } else {
            SubNodes[nodeId].CmdProp.Content = *rxDataBufPtr++;
            memcpy(&SubNodes[nodeId].CmdData, rxDataBufPtr, sizeof(CMD_DATA));

            Eeprom_ReadWrite((uint8 *)meterBufPtr, nodeId * NODE_INFO_SIZE, meterDataLen, READ_ATTR);
            if (0 != memcmp(SubNodes[nodeId].LongAddr, meterBufPtr->Address, LONG_ADDR_SIZE)) {
                Data_MeterDataInit(meterBufPtr, nodeId, meterDataLen);
            }

            if ( (meterBufPtr->CmdProp.Content != SubNodes[nodeId].CmdProp.Content) ||
                (0 == memcmp(&meterBufPtr->CmdData, &SubNodes[nodeId].CmdData, sizeof(CMD_DATA)))) {
                meterBufPtr->CmdProp.Content = SubNodes[nodeId].CmdProp.Content;
                memcpy(&meterBufPtr->CmdData, &SubNodes[nodeId].CmdData, sizeof(CMD_DATA));
                Eeprom_ReadWrite((uint8 *)meterBufPtr, nodeId * NODE_INFO_SIZE, meterDataLen, WRITE_ATTR);
            }

            rxDataBufPtr += sizeof(CMD_DATA);
            *txDataBufPtr++ = OP_Succeed;
        }
    }

	OSMemPut(SmallMemoryPtr, meterBufPtr);
    OSMemPut(LargeMemoryPtr, msg);
    //Flash_SaveSubNodesInfo();
    DataFrmPtr->DataLen = txDataBufPtr - DataFrmPtr->DataBuf;
}


/************************************************************************************************
* Function Name: Data_GprsParameter
* Decription   : ��ȡ������Gprs�Ĳ�����Ϣ
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       :
*   ������:��������
*   ������:IP(4)+PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
*   д����:IP(4)+PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
*   д����:����״̬(1)
************************************************************************************************/
void Data_GprsParameter(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 *msg, *dataBufPtr;
    uint16 dataLen;
    GPRS_PARAMETER *gprsParamPtr;

    dataBufPtr = DataFrmPtr->DataBuf;
    gprsParamPtr = &Concentrator.GprsParam;
    msg = OSMemGetOpt(SmallMemoryPtr, 10, TIME_DELAY_MS(50));
    if ((void *)0 != msg) {
        // ��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+��Ϣ�������(1)+APN����(1)+APN(N)+�û�������(1)+�û���(N)+���볤��(1)+����(N)
        memcpy(msg, gprsParamPtr->PriorDscIp, 4);
        ((uint16 *)(&msg[4]))[0] = gprsParamPtr->PriorDscPort;
        memcpy(&msg[6], gprsParamPtr->BackupDscIp, 4);
        ((uint16 *)(&msg[10]))[0] = gprsParamPtr->BackupDscPort;
        msg[12] = gprsParamPtr->HeatBeatPeriod;
        dataLen = 13;
        if ((msg[dataLen++] = strlen(gprsParamPtr->Apn)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Apn);
            dataLen += msg[dataLen - 1];
        }
        if ((msg[dataLen++] = strlen(gprsParamPtr->Username)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Username);
            dataLen += msg[dataLen - 1];
        }
        if ((msg[dataLen++] = strlen(gprsParamPtr->Password)) > 0) {
            strcpy((char *)(msg + dataLen), gprsParamPtr->Password);
            dataLen += msg[dataLen - 1];
        }
        if (Read_GPRS_Param == DataFrmPtr->Command) {
            // ����:��������
            // ����:��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+����(1)+APN(n)+�û���(n)+����(n)
            memcpy(dataBufPtr, msg, dataLen);
        } else if(Write_GPRS_Param == DataFrmPtr->Command) {
            // ����:��ѡIP(4)+��ѡPORT(2)+����IP(4)+����PORT(2)+����(1)+APN����(1)+APN(n)+�û�������(1)+�û���(n)+���볤��(1)+����(n)
            // ����:����״̬(1)
            if (0 == memcmp(dataBufPtr, msg, dataLen) && dataLen == DataFrmPtr->DataLen) {
                *dataBufPtr = OP_Succeed;
            } else {
                memcpy(gprsParamPtr->PriorDscIp, dataBufPtr, 4);
                gprsParamPtr->PriorDscPort = *(uint16 *)(dataBufPtr + 4);
                memcpy(gprsParamPtr->BackupDscIp, dataBufPtr + 6, 4);
                gprsParamPtr->BackupDscPort = *(uint16 *)(dataBufPtr + 10);
                gprsParamPtr->HeatBeatPeriod = *(dataBufPtr + 12);
                if (gprsParamPtr->HeatBeatPeriod < 3) {
                    gprsParamPtr->HeatBeatPeriod = 3;
                }
                dataLen = 13;
                memset(gprsParamPtr->Apn, 0, sizeof(gprsParamPtr->Apn));
                memcpy(gprsParamPtr->Apn, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                dataLen += *(dataBufPtr + dataLen) + 1;
                memset(gprsParamPtr->Username, 0, sizeof(gprsParamPtr->Username));
                memcpy(gprsParamPtr->Username, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                dataLen += *(dataBufPtr + dataLen) + 1;
                memset(gprsParamPtr->Password, 0, sizeof(gprsParamPtr->Password));
                memcpy(gprsParamPtr->Password, dataBufPtr + dataLen + 1, *(dataBufPtr + dataLen));
                Flash_SaveConcentratorInfo();
                *dataBufPtr = OP_Succeed;
                DevResetTimer = 10000;      // 10���Ӻ��豸��λ
            }
            dataLen = 1;
        }
        OSMemPut(SmallMemoryPtr, msg);
    } else {
        *dataBufPtr = OP_Failure;
        dataLen = 1;
    }
    DataFrmPtr->DataLen = dataLen;
}

/************************************************************************************************
* Function Name: Data_SwUpdate
* Decription   : ������������
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����: Crc16(2)+д���ַ(4)+���������ܳ���(4)+�����������볤��(2)+��������(N)
*                ����: Crc16(2)+д���ַ(4)+�������(1)
************************************************************************************************/
void Data_SwUpdate(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint16 crc16, pkgCodeLen;
    uint32 writeAddr, codeLength;
    uint8 *codeBufPtr, *dataBufPtr;
    uint8 buf[12];

    // ��ȡ����
    dataBufPtr = DataFrmPtr->DataBuf;
    crc16 = ((uint16 *)dataBufPtr)[0];
    writeAddr = ((uint32 *)(dataBufPtr + 2))[0];
    codeLength = ((uint32 *)(dataBufPtr + 6))[0];
    pkgCodeLen = ((uint16 *)(dataBufPtr + 10))[0];
    codeBufPtr = dataBufPtr + 12;

    // ����������볤�ȴ���
    if (codeLength > FLASH_UPGRADECODE_SIZE * FLASH_PAGE_SIZE) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // ����յ���д���ַΪ0,��ʾ��һ���µ�����Ҫ����
    if (0 == writeAddr) {
        Flash_Erase(FLASH_UPGRADECODE_START_ADDR, FLASH_UPGRADECODE_SIZE);
        Flash_Erase(FLASH_UPGRADE_INFO_START, FLASH_UPGRADE_INFO_SIZE);
        // ������Ϣ�����ʽ: Crc16(2)+�����ļ�����λ��(4)+���������ܳ���(4)+Crc16(2)
        memcpy(buf, dataBufPtr, sizeof(buf));
        ((uint32 *)(&buf[2]))[0] = FLASH_UPGRADECODE_START_ADDR;
        ((uint16 *)(&buf[10]))[0] = CalCrc16(buf, 10);
        Flash_Write(buf, 16, FLASH_UPGRADE_INFO_START);
    }

    // ��������У���ֽڻ����������ܳ��ȴ����򷵻ش���
    if (crc16 != ((uint16 *)FLASH_UPGRADE_INFO_START)[0] ||
        codeLength != ((uint32 *)(FLASH_UPGRADE_INFO_START + 6))[0]) {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // д����������
    if (codeLength >= writeAddr + pkgCodeLen) {
        if (0 != memcmp(codeBufPtr, (uint8 *)(FLASH_UPGRADECODE_START_ADDR + writeAddr), pkgCodeLen)) {
            Flash_Write(codeBufPtr, pkgCodeLen, FLASH_UPGRADECODE_START_ADDR + writeAddr);
            if (0 != memcmp(codeBufPtr, (uint8 *)(FLASH_UPGRADECODE_START_ADDR + writeAddr), pkgCodeLen)) {
                *(dataBufPtr + 6) = OP_Failure;
                DataFrmPtr->DataLen = 7;
                return;
            }
        }
    } else {
        *(dataBufPtr + 6) = OP_ParameterError;
        DataFrmPtr->DataLen = 7;
        return;
    }

    // ����Ƿ������һ��
    if (writeAddr + pkgCodeLen >= codeLength) {
        if (crc16 == CalCrc16((uint8 *)FLASH_UPGRADECODE_START_ADDR, codeLength)) {
            *(dataBufPtr + 6) = OP_Succeed;
            DevResetTimer = 10000;      // 10���Ӻ��豸��λ
        } else {
            *(dataBufPtr + 6) = OP_Failure;
        }
    } else {
        *(dataBufPtr + 6) = OP_Succeed;
    }
    DataFrmPtr->DataLen = 7;
    return;
}

/************************************************************************************************
* Function Name: Data_EepromCheckProc
* Decription   : Eeprom�洢оƬ������
* Input        : DataFrmPtr-ָ������֡��ָ��
* Output       : ��
* Others       : ����: ��������
*                ����: �������(1)+Eeprom����(1)+����λ��(4)
************************************************************************************************/
void Data_EepromCheckProc(DATA_FRAME_STRUCT *DataFrmPtr)
{
    uint8 i, retry, type, *msg, *buf, *dataBufPtr;
    uint16 j, err[2];
    uint32 addr;

    DataFrmPtr->DataLen = 6;
    dataBufPtr = DataFrmPtr->DataBuf;
#ifdef EEPROM_SEL_M512
    type = 0;
#endif
#ifdef EEPROM_SEL_M01
    type = 1;
#endif
    msg = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50));
    buf = OSMemGetOpt(LargeMemoryPtr, 10, TIME_DELAY_MS(50));
    if ((void *)0 == msg || (void *)0 == buf) {
        *dataBufPtr++ = OP_Failure;
        *dataBufPtr++ = type;
        *dataBufPtr++ = 0xAA;
        *dataBufPtr++ = 0xAA;
        *dataBufPtr++ = 0xAA;
        *dataBufPtr++ = 0xAA;
    } else {
        addr = 0;
        while (addr < EEPROM_TOTALSIZE) {
            Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay100ms, FALSE);
            Eeprom_ReadWrite(msg, addr, MEM_LARGE_BLOCK_LEN, READ_ATTR);
            for (i = 0; i < 2; i++) {
                for (j = 0; j < MEM_LARGE_BLOCK_LEN; j++) {
                    *(msg + j) ^= 0xFF;
                }
                for (retry = 0; retry < 3; retry++) {
                    Eeprom_ReadWrite(msg, addr, MEM_LARGE_BLOCK_LEN, WRITE_ATTR);
                    Eeprom_ReadWrite(buf, addr, MEM_LARGE_BLOCK_LEN, READ_ATTR);
                    err[i] = 0xFFFF;
                    for (j = 0; j < MEM_LARGE_BLOCK_LEN; j++) {
                        if (*(msg + j) != *(buf + j)) {
                            err[i] = j;
                            break;
                        }
                    }
                    if (0xFFFF == err[i]) {
                        break;
                    }
                }
            }
            if (0xFFFF != err[0] || 0xFFFF != err[1]) {
                break;
            }
            addr += MEM_LARGE_BLOCK_LEN;
        }
        if (0xFFFF != err[0] || 0xFFFF != err[1]) {
            addr += err[0] < err[1] ? err[0] : err[1];
            *dataBufPtr++ = OP_Failure;
        } else {
            addr = 0xFFFFFFFF;
            *dataBufPtr++ = OP_Succeed;
        }
        *dataBufPtr++ = type;
        *dataBufPtr++ = (uint8)(addr >> 0);
        *dataBufPtr++ = (uint8)(addr >> 8);
        *dataBufPtr++ = (uint8)(addr >> 16);
        *dataBufPtr++ = (uint8)(addr >> 24);
    }
    Led_FlashTime(WORKSTATUS_LED, Delay100ms, Delay3000ms, FALSE);
    if ((void *)0 != msg) {
        OSMemPut(LargeMemoryPtr, msg);
    }
    if ((void *)0 != buf) {
        OSMemPut(LargeMemoryPtr, buf);
    }
}

/***************************************End of file*********************************************/

