
/*--------------------------------------------------------------- 
* Initialise IDAM Hierarchical XML Data Structures 
* 
* Input Arguments:	  
* 
* Returns:		  
* 
* Calls		 
* 
* Notes:  	 
* 
* ToDo:		                        
* 
* Change History 
* 
* 25Jun2007	D.G.Muir
* 13May2008	D.G.Muir	Test for NULL structures in Print functions	 
* 
*--------------------------------------------------------------*/
#include "efitmagxml.h"

#include <idampluginfiles.h>
#include <idamserver.h>
#include <idamErrorLog.h>
#include <managePluginFiles.h>
#include <initStructs.h>
#include <makeServerRequestBlock.h>
#include <client/accAPI_C.h>
#include <client/IdamAPI.h>
#include <freeDataBlock.h>
#include <structures/struct.h>
#include <structures/accessors.h>
#include <clientserver/TrimString.h>
#include <clientserver/idamErrors.h> 

void freeEfit(EFIT* efit)
{

    int i, nel;

    if (efit->magprobe != NULL) free((void*) efit->magprobe);
    //if(efit->polarimetry    != NULL) free((void *)efit->polarimetry);
    //if(efit->interferometry != NULL) free((void *)efit->interferometry);
    if (efit->diamagnetic != NULL) free((void*) efit->diamagnetic);

    nel = efit->nfluxloops;
    if (nel > 0) {
        for (i = 0; i < nel; i++) {
            if (efit->fluxloop[i].r != NULL) free((void*) efit->fluxloop[i].r);
            if (efit->fluxloop[i].z != NULL) free((void*) efit->fluxloop[i].z);
            if (efit->fluxloop[i].dphi != NULL) free((void*) efit->fluxloop[i].dphi);
        }
    }
    if (efit->fluxloop != NULL) free((void*) efit->fluxloop);

    if ((nel = efit->npfpassive) > 0) {
        for (i = 0; i < nel; i++) {
            if (efit->pfpassive[i].r != NULL) free((void*) efit->pfpassive[i].r);
            if (efit->pfpassive[i].z != NULL) free((void*) efit->pfpassive[i].z);
            if (efit->pfpassive[i].dr != NULL) free((void*) efit->pfpassive[i].dr);
            if (efit->pfpassive[i].dz != NULL) free((void*) efit->pfpassive[i].dz);
            if (efit->pfpassive[i].ang1 != NULL) free((void*) efit->pfpassive[i].ang1);
            if (efit->pfpassive[i].ang2 != NULL) free((void*) efit->pfpassive[i].ang2);
            if (efit->pfpassive[i].res != NULL) free((void*) efit->pfpassive[i].res);
        }
    }
    if (efit->pfpassive != NULL) free((void*) efit->pfpassive);

    if ((nel = efit->npfcoils) > 0) {
        for (i = 0; i < nel; i++) {
            if (efit->pfcoils[i].r != NULL) free((void*) efit->pfcoils[i].r);
            if (efit->pfcoils[i].z != NULL) free((void*) efit->pfcoils[i].z);
            if (efit->pfcoils[i].dr != NULL) free((void*) efit->pfcoils[i].dr);
            if (efit->pfcoils[i].dz != NULL) free((void*) efit->pfcoils[i].dz);
        }
    }
    if (efit->pfcoils != NULL) free((void*) efit->pfcoils);

    if ((nel = efit->npfcircuits) > 0) {
        for (i = 0; i < nel; i++) {
            if (efit->pfcircuit[i].coil != NULL) free((void*) efit->pfcircuit[i].coil);
        }
    }

    if (efit->pfcircuit != NULL) free((void*) efit->pfcircuit);
    if (efit->pfsupplies != NULL) free((void*) efit->pfsupplies);
    if (efit->plasmacurrent != NULL) free((void*) efit->plasmacurrent);
    if (efit->toroidalfield != NULL) free((void*) efit->toroidalfield);

    if (efit->limiter != NULL) {
        if (efit->nlimiter) {
            if (efit->limiter->r != NULL) free((void*) efit->limiter->r);
            if (efit->limiter->z != NULL) free((void*) efit->limiter->z);
        }
        free((void*) efit->limiter);
    }
#ifdef JETMSEXML
    if(efit->mse > 0){
       if(efit->mse->weight != NULL) free((void *)efit->mse->weight);
       if(efit->mse->r      != NULL) free((void *)efit->mse->r);
       if(efit->mse->z      != NULL) free((void *)efit->mse->z);
       if(efit->mse->hxvr   != NULL) free((void *)efit->mse->hxvr);
       if(efit->mse->hxvf   != NULL) free((void *)efit->mse->hxvf);
       if(efit->mse->hxvz   != NULL) free((void *)efit->mse->hxvz);
       if(efit->mse->vxvr   != NULL) free((void *)efit->mse->vxvr);
       if(efit->mse->vxvf   != NULL) free((void *)efit->mse->vxvf);
       if(efit->mse->vxvz   != NULL) free((void *)efit->mse->vxvz);
    }
    if(efit->mse != NULL) free((void *)efit->mse);
    efit->mse = NULL;

    if(efit->mse_info.signal_list != NULL){
       free((void *)efit->mse_info.signal_list);
       efit->mse_info.signal_list = NULL;
       efit->mse_info.signal_number = 0;
    }
#endif

    initEfit(efit);

}

void initEfit(EFIT* str)
{
    str->device[0] = '\0';
    str->exp_number = 0;
    str->nfluxloops = 0;
    str->nmagprobes = 0;
    str->npfcircuits = 0;
    str->npfpassive = 0;
    str->nplasmacurrent = 0;
    str->ndiamagnetic = 0;
    str->ntoroidalfield = 0;
    str->npfsupplies = 0;
    str->npfcoils = 0;
    str->nlimiter = 0;

    str->fluxloop = NULL;
    str->pfpassive = NULL;
    str->magprobe = NULL;
    str->pfcircuit = NULL;
    str->plasmacurrent = NULL;
    str->diamagnetic = NULL;
    str->toroidalfield = NULL;
    str->pfsupplies = NULL;
    str->pfcoils = NULL;
    str->limiter = NULL;
}

void initInstance(INSTANCE* str)
{
    str->archive[0] = '\0';
    str->file[0] = '\0';
    str->signal[0] = '\0';
    str->owner[0] = '\0';
    str->format[0] = '\0';
    str->status = 0;
    str->seq = 0;
    str->factor = 1.0;    // Factor Always applied to Data!
}

void initFluxLoop(FLUXLOOP* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->nco = 0;
    str->r = NULL;
    str->z = NULL;
    str->dphi = NULL;
    str->aerr = 0.0;
    str->rerr = 0.0;
}

void initPfPassive(PFPASSIVE* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->nco = 0;
    str->r = NULL;
    str->z = NULL;
    str->dr = NULL;
    str->dz = NULL;
    str->ang1 = NULL;
    str->ang2 = NULL;
    str->res = NULL;
    str->modelnrnz[0] = 0;
    str->modelnrnz[1] = 0;
}

void initPfCoils(PFCOILS* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->aerr = 0.0;
    str->rerr = 0.0;
    str->turns = 0;
    str->nco = 0;
    str->r = NULL;
    str->z = NULL;
    str->dr = NULL;
    str->dz = NULL;
    str->modelnrnz[0] = 0;
    str->modelnrnz[1] = 0;
}

void initMagProbe(MAGPROBE* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->r = 0.0;
    str->z = 0.0;
    str->angle = 0.0;
    str->aerr = 0.0;
    str->rerr = 0.0;
}


void initPfSupplies(PFSUPPLIES* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->aerr = 0.0;
    str->rerr = 0.0;
}

void initPfCircuits(PFCIRCUIT* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->nco = 0;
    str->coil = NULL;
    str->supply = 0;
}

void initPlasmaCurrent(PLASMACURRENT* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->aerr = 0.0;
    str->rerr = 0.0;
}

void initDiaMagnetic(DIAMAGNETIC* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->aerr = 0.0;
    str->rerr = 0.0;
}

void initToroidalField(TOROIDALFIELD* str)
{
    str->id[0] = '\0';
    initInstance(&str->instance);
    str->aerr = 0.0;
    str->rerr = 0.0;
}

void initLimiter(LIMITER* str)
{
    str->nco = 0;
    str->factor = 1.0;        // Unique as No Instance Child Structure
    str->r = NULL;
    str->z = NULL;
}

// Print Utilities 

void printInstance(FILE* fh, INSTANCE str)
{
    if (&str == NULL) return;
    fprintf(fh, "archive       : %s\n", str.archive);
    fprintf(fh, "file          : %s\n", str.file);
    fprintf(fh, "signal        : %s\n", str.signal);
    fprintf(fh, "owner         : %s\n", str.owner);
    fprintf(fh, "format        : %s\n", str.format);
    fprintf(fh, "sequence/pass : %d\n", str.seq);
    fprintf(fh, "status        : %d\n", str.status);
    fprintf(fh, "factor        : %f\n", str.factor);
    return;
}

void printMagProbe(FILE* fh, MAGPROBE str)
{
    if (&str == NULL) return;
    fprintf(fh, "Magnetic Probe\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "r          : %f\n", str.r);
    fprintf(fh, "z          : %f\n", str.z);
    fprintf(fh, "angle      : %f\n", str.angle);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    return;
}

void printPfSupplies(FILE* fh, PFSUPPLIES str)
{
    if (&str == NULL) return;
    fprintf(fh, "PF Supply\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    return;
}


void printPfCircuits(FILE* fh, PFCIRCUIT str)
{
    int i;
    if (&str == NULL) return;
    fprintf(fh, "PF Circuit\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "supply     : %d\n", str.supply);
    fprintf(fh, "nco        : %d\n", str.nco);
    for (i = 0; i < str.nco; i++) fprintf(fh, "Coil Connect # %d     : %d\n", i, str.coil[i]);
    return;
}

void printFluxLoop(FILE* fh, FLUXLOOP str)
{
    int i;
    if (&str == NULL) return;
    fprintf(fh, "Flux Loop\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    fprintf(fh, "nco        : %d\n", str.nco);
    if (str.nco > 0) {
        for (i = 0; i < str.nco; i++)
            fprintf(fh, "r, z, dphi   # %d     : %f   %f   %f\n", i, str.r[i], str.z[i], str.dphi[i]);
    }
    return;
}

void printPfCoils(FILE* fh, PFCOILS str)
{
    int i;
    if (&str == NULL) return;
    fprintf(fh, "PF Coil\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    fprintf(fh, "turns per  : %d\n", str.turns);
    fprintf(fh, "turns per  : %f\n", str.fturns);
    fprintf(fh, "model nr nr: %d  %d\n", str.modelnrnz[0], str.modelnrnz[1]);
    fprintf(fh, "nco        : %d\n", str.nco);
    for (i = 0; i < str.nco; i++)
        fprintf(fh, "r, z, dr, dz # %d     : %f   %f   %f   %f\n", i, str.r[i], str.z[i], str.dr[i], str.dz[i]);
    return;
}

void printPfPassive(FILE* fh, PFPASSIVE str)
{
    int i;
    if (&str == NULL) return;
    fprintf(fh, "PF Passive\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    fprintf(fh, "model nr nr: %d  %d\n", str.modelnrnz[0], str.modelnrnz[1]);
    for (i = 0; i < str.nco; i++)
        fprintf(fh, "r,z,dr,dz,a1,a2,res   # %d     : %f  %f  %f  %f  %f  %f  %f\n", i, str.r[i], str.z[i],
                str.dr[i], str.dz[i], str.ang1[i], str.ang2[i], str.res[i]);
    return;
}

void printPlasmaCurrent(FILE* fh, PLASMACURRENT str)
{
    if (&str == NULL) return;
    fprintf(fh, "Plasma Current\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    return;
}

void printDiaMagnetic(FILE* fh, DIAMAGNETIC str)
{
    if (&str == NULL) return;
    fprintf(fh, "Diamagnetic Flux\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    return;
}

void printToroidalField(FILE* fh, TOROIDALFIELD str)
{
    if (&str == NULL) return;
    fprintf(fh, "Toroidal Field\n");
    fprintf(fh, "id         : %s\n", str.id);
    printInstance(fh, str.instance);
    fprintf(fh, "aerr       : %f\n", str.aerr);
    fprintf(fh, "rerr       : %f\n", str.rerr);
    return;
}

void printLimiter(FILE* fh, LIMITER str)
{
    int i;
    if (&str == NULL) return;
    fprintf(fh, "Limiter\n");
    fprintf(fh, "factor     : %f\n", str.factor);
    fprintf(fh, "nco        : %d\n", str.nco);
    for (i = 0; i < str.nco; i++) fprintf(fh, "r, z   # %d     : %f    %f\n", i, str.r[i], str.z[i]);
    return;
}

void printEFIT(FILE* fh, EFIT str)
{
    fprintf(fh, "EFIT Hierarchical Structure\n");
    fprintf(fh, "Device     : %s\n", str.device);
    fprintf(fh, "Exp. Number: %d\n", str.exp_number);

    if (str.fluxloop != NULL) printFluxLoop(fh, *(str.fluxloop));
    if (str.magprobe != NULL) printMagProbe(fh, *(str.magprobe));
    if (str.pfcircuit != NULL) printPfCircuits(fh, *(str.pfcircuit));
    if (str.pfpassive != NULL) printPfPassive(fh, *(str.pfpassive));
    if (str.plasmacurrent != NULL) printPlasmaCurrent(fh, *(str.plasmacurrent));
    if (str.toroidalfield != NULL) printToroidalField(fh, *(str.toroidalfield));
    if (str.pfsupplies != NULL) printPfSupplies(fh, *(str.pfsupplies));
    if (str.pfcoils != NULL) printPfCoils(fh, *(str.pfcoils));
    if (str.limiter != NULL) printLimiter(fh, *(str.limiter));
    if (str.diamagnetic != NULL) printDiaMagnetic(fh, *(str.diamagnetic));
} 
