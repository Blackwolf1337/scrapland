/*
 * cNetVController of the game (rva 0x1186b0..0x119850, between VehicleMotorsEffects.cpp and VehicleTraffic.cpp; the
 * source file has no CHECK string, its name is not known): the network controller of a vehicle, a cNetController
 * (ObjEntity.c) with the vehicle, the hits and the controls of the client. See include/scrap/vehicleentity.h.
 */
#include "scrap/objentity.h"
#include "scrap/vehicleentity.h"

#define CNETVCONTROLLER_VFTABLE 0x3b61b8

cNetVController *__thiscall cNetVController_ctor(void *mem, cEntity *v) {
	cNetVController *self = (cNetVController *)cNetController_ctor(mem);

	self->vehicle = v;
	self->vftable = Game_Ptr(CNETVCONTROLLER_VFTABLE);
	self->hit = 0;
	self->force = 0;
	self->b60 = 0;
	self->i6c = 0;
	self->height = 0.0f;
	return self;
}

/* vf03: an active vehicle always sends its position */
byte __thiscall cNetVController_Changed(cNetVController *self, byte flags, cEntity *e) {
	if (self->vehicle->flag_4c) {
		byte f = cNetController_Changed((cNetController *)self, flags, e) | NETC_POS;
		self->flags = f;
		return f;
	}
	return cNetController_Changed((cNetController *)self, flags, e);
}

/* vf00 */
void __thiscall cNetVController_Reset(cNetVController *self) {
	cNetController_Reset((cNetController *)self);
}
