#include "settings.h"
#include "eeprom.h"

uint16_t VirtAddVarTab[NB_OF_VAR] = {0x5555};
uint16_t VarDataTab[NB_OF_VAR] = {0};

const uint16_t defaultLim = 1250;

unsigned short getLimLevel(void)
{
    uint16_t status = EE_ReadVariable(VirtAddVarTab[0], &VarDataTab[0]);
    if(status!=0) {EE_WriteVariable(VirtAddVarTab[0],defaultLim);return defaultLim;}
    return VarDataTab[0];
}

void setLimLevel(unsigned short value)
{
    VarDataTab[0] = value;
    EE_WriteVariable(VirtAddVarTab[0], VarDataTab[0]);
}

void initSettings(void)
{
    FLASH_Unlock();
    EE_Init();
}
