#ifndef POWER_UPS_H
#define POWER_UPS_H

#include "hieri_types.h"

class PowerUps {
public:
    static PowerUps instance;

    int getCloakTime(void);
    void update(void);
    void initPowerUpsAfter(void);
    void KillPowerUps(void);
    void CreatePowerUp(int type, unsigned char arg, _fvector *pos);
    void startCinema(void);
    void endCinema(void);
};

#endif
