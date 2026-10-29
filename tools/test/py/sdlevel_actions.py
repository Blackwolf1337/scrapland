# Actions/ (the action classes, actions, events and controllers), Actions/Events/ and the small AI files (AI.cpp,
# AIBankDirector, AIBankMaster, AIBishop). Read only on the real objects: the class and the controller of Player0 and
# its actions (no "Event" variables there: the last event of an action is not set until AddEventAtTime). The writes on
# our own objects: a class "SDActCls" with three actions and their events in the real class list (deleted at the end;
# the anmName and the last event of a new action are not set by the constructor, so they are cleared first), a copy of
# the controller of Player0 in calloc memory with our class and actions in its layers (only the paths that do not
# reach the model: the priority refusals, the default loops, the events), a list of our own and the pure functions.
# The C event functions run through events with "!name" functions on Player0 where they do not change it (missing
# actions, a missing sound) or with the values saved and restored. Not called: the model parts of the controller
# (ctor, Update, Tick, TurnHead, PlayAction past the refusal, UpdateShip), the bank and bishop AIs past their ranges
# (they need a cAIChar with its control) and the voice events (they open a voice stream).
def acti(a):
	return SDTest.Peek(a, '@i')
def actb(a):
	return SDTest.Peek(a, '@b')
def actpoke(a, v):
	SDTest.Poke(a - 0x400000, v)
def actfl(a, n):
	return SDTest.Call('crt_memcpy', 'iFii', (0.0,) * n, a, 4 * n)[1]
def actsetfl(a, v):
	SDTest.Call('crt_memcpy', 'iiFi', a, v, 4 * len(v))
def actz(n):
	return SDTest.Call('Mem_Calloc', 'ii', n)[0]
def actfree(p):
	SDTest.Call('Mem_Free', 'ii', p)
def actstr(a):
	if a:
		return SDTest.Peek(a, '@s')
	return None
# a string in calloc memory (ASCII, 4 characters an int)
def acttext(s):
	p = actz(len(s) + 8)
	j = 0
	while j < len(s):
		v = 0
		k = 0
		while k < 4 and j + k < len(s):
			v = v | (ord(s[j + k]) << (8 * k))
			k = k + 1
		actpoke(p + j, v)
		j = j + 4
	return p

# the Var functions (name, set, s, d, i): a read with all three pointers, the value by the type returned
ACT_CLSVAR = sym('cActClass_Var')
ACT_CAMVAR = sym('cActClassCam_Var')
ACT_ACTVAR = sym('cAction_Var')
ACT_EVVAR = sym('cActEvent_Var')
def actget(fn, obj, name):
	r = SDTest.Call(fn, 'tiisipDp', obj, name, 0, 0, -1.0, -7)
	if r[0] == 1:
		return (1, actstr(r[1]))
	if r[0] == 2:
		return (2, r[2])
	if r[0] == 3:
		return (3, r[3])
	return (r[0], r[1] == 0, r[2], r[3])
# a number set like SAct.SetCls: s NULL, the double and its int
def actsetd(fn, obj, name, d):
	return SDTest.Call(fn, 'tiisiiDp', obj, name, 1, 0, d, int(d))[0]
def actsets(fn, obj, name, text):
	p = acttext(text)
	r = SDTest.Call(fn, 'tiisipii', obj, name, 1, p, 0, 0)[0]
	actfree(p)
	return r
# the events of an action: time, name, discardable
def actevents(a):
	r = ()
	e = acti(a + 0x24)
	n = 0
	while e and n < 20:
		r = r + ((actfl(e, 1)[0], actstr(acti(e + 4)), actb(e + 0x20)),)
		e = acti(e + 0x1c)
		n = n + 1
	return r
def actevname(e):
	if e:
		return actstr(acti(e + 4))
	return None

actworld = SDTest.Peek('g_pWorld', 'i')
actlist = SDTest.Peek('g_pActClassList', 'i')
actplayer = SDTest.Call('cWorld_GetUsrEntity', 'tiii', actworld, 0)[0]
actpc = 0
actcls = 0
if actplayer:
	actpc = SDTest.Call(acti(acti(actplayer) + 0xd4) - 0x400000, 'tii', actplayer)[0]
if actpc:
	actcls = acti(actpc + 8)
check('ACT objects', lambda: (actlist != 0, actplayer != 0, actpc != 0, actcls != 0,
	actlist and acti(actlist) - 0x400000, actpc and acti(actpc) - 0x400000))

actclsvars = ('Name', 'DefAction', 'ModelFilePath', 'SoundDispX', 'SoundDispY', 'SoundDispZ', 'FallSound',
	'FallSoundVol', 'SkipInitOnChange', 'DinaShip', 'Discardable', 'CanBePossessed', 'RelaxAlarm', 'FireAsSelect',
	'StrafeChar', 'PretransformChan', 'CanFly', 'CanJump', 'InitEndWalkInterpFactor', 'YModelDisp', 'MinTimeWalking',
	'OnCreate', 'FlyFallAccel', 'FlySpeed', 'FlyFric', 'FlyAccel', 'SRadius', 'VRadius', 'WRadius', 'MinFallDist',
	'MaxTimeToExtraRelax', 'MinTimeToExtraRelax', 'TurnAngLimit', 'CamDist', 'CamAng', 'CamRot', 'FOV', 'MinFOV',
	'MaxFOV', 'FOVInterpTime', 'Node0_Name', 'Node0_0x', 'name', 'Missing')
actactvars = ('Name', 'AnmFilePath', 'AnmFPS', 'AnmTotalTime', 'isLoop', 'MaxSpeed', 'MinSpeed', 'RotSpeed',
	'InterpTime', 'PriorityLevel', 'Chanel', 'NodeModifier', 'AngRotLimit', 'AngleModifierSpeed', 'AutoFaceSpeed',
	'AutoFaceOffsetH', 'AutoFaceOffsetV', 'UseDisp', 'UseFlyDisp', 'UseFlyFall', 'SlideAsFly', 'anmConeView',
	'Missing')

# ---- the class of Player0 ----
if actcls:
	actclsname = actstr(acti(actcls + 4))
	check('ACT list Get', lambda: (SDTest.Call('cActClassList_Get', 'tiis', actlist, actclsname)[0] == actcls,
		SDTest.Call('cActClassList_Get', 'tiis', actlist, 'SDActNoClass')[0]))
	check('ACT list Del in use', lambda: (acti(actcls + 0x58) > 0) and SDTest.Call('cActClassList_Del', 'tbis', actlist,
		actclsname)[0])
	check('ACT list Del missing', lambda: SDTest.Call('cActClassList_Del', 'tbis', actlist, 'SDActNoClass')[0])
	check('ACT Init again', lambda: (SDTest.Call('ActClassList_Init', 'v'), SDTest.Peek('g_pActClassList', 'i') == actlist))
	for actn in actclsvars:
		check('ACT cls %s' % actn, lambda n=actn: actget(ACT_CLSVAR, actcls, n))
	for actn in ('CamDist', 'CamAng', 'FOV', 'MinFOV', 'MaxFOV', 'FOVInterpTime', 'camdist', 'Name'):
		check('ACT cam %s' % actn, lambda n=actn: actget(ACT_CAMVAR, actcls + 0x110, n))
	check('ACT cam no d', lambda: SDTest.Call(ACT_CAMVAR, 'tiisiiii', actcls + 0x110, 'CamDist', 0, 0, 0, 0)[0])
	actdef = acti(actcls + 0x20)
	if actdef:
		check('ACT GetAction def', lambda: SDTest.Call('cActClass_GetAction', 'tiis', actcls, actstr(acti(actdef + 8)))[0] ==
			actdef)
	check('ACT GetAction missing', lambda: SDTest.Call('cActClass_GetAction', 'tiis', actcls, 'SDActX')[0])
	actnode = actz(8)
	check('ACT SetNode', lambda: (SDTest.Call('cActNodeMod_SetNode', 'tviis', actnode, actcls + 0x24, '_Move_The_Model_'),
		acti(actnode)))
	if acti(actcls + 0x24):
		check('ACT SetNode model', lambda: (SDTest.Call('cActNodeMod_SetNode', 'tviis', actnode, actcls + 0x24, '_Move_The_Model'),
			acti(actnode), SDTest.Call('cActNodeMod_SetNode', 'tviis', actnode, actcls + 0x24, 'SDActNoNode'), acti(actnode)))
	actfree(actnode)
	# the actions of the class (AnmName only where an animation was set; the frame times only with the frames of the
	# animations as they are, AnimBaseFPS 0: else a frame out of range is a fatal error)
	actbase = actfl(actcls + 0x38, 1)[0]
	check('ACT cls AnimBaseFPS', lambda: actbase)
	acta = acti(actcls + 0x14)
	actk = 0
	while acta and actk < 6:
		for actn in actactvars:
			check('ACT act%d %s' % (actk, actn), lambda a=acta, n=actn: actget(ACT_ACTVAR, a, n))
		if acti(acta + 0xc):
			check('ACT act%d AnmName' % actk, lambda a=acta: actget(ACT_ACTVAR, a, 'AnmName'))
			if actbase == 0.0 and acti(acta + 4) >= 0:
				for actn in ('TimeOfFrame0', 'TimeOfFrame1', 'TimeOfFrame99999', 'TimeOfFrame-1', 'TimeOfFrame'):
					check('ACT act%d %s' % (actk, actn), lambda a=acta, n=actn: actget(ACT_ACTVAR, a, n))
		acta = acti(acta + 0x18)
		actk = actk + 1

# ---- the controller of Player0, read only ----
if actpc:
	check('ACT pc GetLayer', lambda: (SDTest.Call('cActCtrl_GetLayer', 'tiii', actpc, 0)[0] == actpc + 0x10,
		SDTest.Call('cActCtrl_GetLayer', 'tiii', actpc, 3)[0] == actpc + 0x40))
	check('ACT pc VarI/F/V read', lambda: (SDTest.Call('cActCtrl_VarI', 'tbispi', actpc, 'StopSecondary', 1, 0)[0:2],
		SDTest.Call('cActCtrl_VarF', 'tbisDi', actpc, 'Action', 1.0, 0)[0:2],
		SDTest.Call('cActCtrl_VarV', 'tbisDDDi', actpc, 'Action', 1.0, 2.0, 3.0, 1)[0]))
	check('ACT pc VarS missing', lambda: (SDTest.Call('cActCtrl_VarS', 'tbispi', actpc, 'Missing', 0, 0),
		SDTest.Call('cActCtrl_VarS', 'tbispi', actpc, 'ActChan4', 0, 0), SDTest.Call('cActCtrl_VarS', 'tbispi', actpc, 'ActChan0',
		0, 1)[0]))

# ---- pure functions ----
for actv in (((1.0, 0.0, 0.0), (0.0, 1.0, 0.0)), ((1.0, 2.0, 3.0), (1.0, 2.0, 3.0)), ((1.0, 2.0, 3.0),
		(-2.0, -4.0, -6.0)), ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0)), ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
		((3.0, -4.0, 0.5), (0.25, 7.0, -2.0)), ((1e-20, 0.0, 0.0), (0.0, 0.0, 1e-20))):
	check('ACT Vec3_Angle %s' % fmt(actv), lambda v=actv: SDTest.Call('Vec3_Angle', 'fFF', v[0], v[1])[0])
for acti_ in range(-1, 12):
	check('ACT AI GetEventName %d' % acti_, lambda i=acti_: SDTest.Call('cAI_GetEventName', 'tviib', 0, i, '')[1])
acth = actz(0x18)
check('ACT cActHead', lambda: (SDTest.Call('cActHead_Set', 'tviF', acth, (0.5, -1.25)), actfl(acth, 6),
	SDTest.Call('cActHead_Set', 'tviF', acth, (2.0, 4.0)), actfl(acth, 6), SDTest.Call('cActHead_Blend', 'tvif', acth, 0.25),
	actfl(acth, 6), SDTest.Call('cActHead_Blend', 'tvif', acth, 1.5), actfl(acth, 6)))
actfree(acth)
actfc = actz(0x80)
check('ACT InitHeads', lambda: (SDTest.Call('cActCtrl_InitHeads', 'tvi', actfc), acti(actfc + 0x58) != 0,
	actfl(acti(actfc + 0x58), 18), SDTest.Call('cActCtrl_InitHeads', 'tvi', actfc), acti(actfc + 0x58) != 0))
SDTest.Call('operator_delete_array', 'vi', acti(actfc + 0x58))
actfree(actfc)
check('ACT AI Print', lambda: SDTest.Call('cAI_Print', 'tvis', 0, 'SDActAI print\n'))

# ---- DList<cString> by hand: three nodes of NewNode, room for two in the free list ----
actsl = actz(0x18)
actpoke(actsl + 0x14, 2)
actn1 = SDTest.Call('cStringList_NewNode', 'tii', actsl)[0]
actn2 = SDTest.Call('cStringList_NewNode', 'tii', actsl)[0]
actn3 = SDTest.Call('cStringList_NewNode', 'tii', actsl)[0]
check('ACT strlist new', lambda: (actn1 != 0, actn2 != 0, actn3 != 0, acti(actn1 + 8), acti(actsl + 4)))
for actp in ((actn1, actn2, actn3), (actn2, actn3, actn1), (actn3, actn1, actn2)):
	actpoke(actp[0], actp[1])
	actpoke(actp[1] + 4, actp[0])
actpoke(actsl, actn1)
actpoke(actsl + 4, actn2)
actpoke(actsl + 8, 3)
check('ACT strlist RemoveHead', lambda: (SDTest.Call('cStringList_RemoveHead', 'tvi', actsl), acti(actsl) == actn2,
	acti(actsl + 4), acti(actsl + 8), acti(actsl + 0xc) == actn1, acti(actsl + 0x10), acti(actn2 + 4) == actn3,
	acti(actn3) == actn2))
check('ACT strlist NewNode free', lambda: (SDTest.Call('cStringList_NewNode', 'tii', actsl)[0] == actn1, acti(actsl + 0xc),
	acti(actsl + 0x10)))
check('ACT strlist Clear', lambda: (SDTest.Call('cStringList_Clear', 'tvi', actsl), acti(actsl), acti(actsl + 8),
	acti(actsl + 0x10), acti(actsl + 0xc) == actn3, acti(actn3) == actn2))
# a one node ring with a full free list: the node is deleted
actpoke(actn1, actn1)
actpoke(actn1 + 4, actn1)
actpoke(actsl, actn1)
actpoke(actsl + 8, 1)
check('ACT strlist RemoveHead last', lambda: (SDTest.Call('cStringList_RemoveHead', 'tvi', actsl), acti(actsl), acti(actsl + 8),
	acti(actsl + 0x10)))
check('ACT strlist FreePool', lambda: (SDTest.Call('cStringList_FreePool', 'tvi', actsl), acti(actsl + 0xc), acti(actsl + 0x10)))
actfree(actsl)

# ---- a class list of our own ----
actown = actz(0x10)
check('ACT own list', lambda: (SDTest.Call('cActClassList_ctor', 'tii', actown)[0] == actown, acti(actown) - 0x400000,
	acti(actown + 4), acti(actown + 8) != 0, actb(actown + 0xc), actb(actown + 0xd)))
actoc = SDTest.Call('cActClassList_Create', 'tiis', actown, 'SDActOwnA')[0]
check('ACT own Create', lambda: (actoc != 0, actstr(acti(actoc + 4)), SDTest.Call('cActClassList_Get', 'tiis', actown,
	'SDActOwnA')[0] == actoc, SDTest.Call('cActClassList_Get', 'tiis', actown, 'sdactowna')[0] == actoc))
check('ACT own Del', lambda: (SDTest.Call('cActClassList_Del', 'tbis', actown, 'SDActOwnA')[0], SDTest.Call('cActClassList_Get', 'tiis',
	actown, 'SDActOwnA')[0], SDTest.Call('cActClassList_Del', 'tbis', actown, 'SDActOwnA')[0]))
actoc = SDTest.Call('cActClassList_Create', 'tiis', actown, 'SDActOwnB')[0]
check('ACT own Delete', lambda: (actoc != 0, SDTest.Call('cActClassList_Delete', 'tiii', actown, 0)[0] == actown,
	acti(actown) - 0x400000, acti(actown + 4), acti(actown + 8)))
actfree(actown)

# ---- our class in the real list ----
actc = 0
if actlist and not SDTest.Call('cActClassList_Get', 'tiis', actlist, 'SDActCls')[0]:
	actc = SDTest.Call('cActClassList_Create', 'tiis', actlist, 'SDActCls')[0]
check('ACT our class', lambda: (actc != 0, actc and SDTest.Call('cActClassList_Get', 'tiis', actlist, 'SDActCls')[0] == actc))
acta1 = 0
acta2 = 0
acta3 = 0
if actc:
	for actn in actclsvars:
		check('ACT new cls %s' % actn, lambda n=actn: actget(ACT_CLSVAR, actc, n))
	acta1 = SDTest.Call('cActClass_CreateAction', 'tiis', actc, 'SDActA')[0]
	acta2 = SDTest.Call('cActClass_CreateAction', 'tiis', actc, 'SDActB')[0]
	acta3 = SDTest.Call('cActClass_CreateAction', 'tiis', actc, 'SDActC')[0]
	for actp in (acta1, acta2, acta3):
		actpoke(actp + 0x10, 0)
		actpoke(actp + 0x28, 0)
	check('ACT actions', lambda: (acta1 != 0, acta2 != 0, acta3 != 0, acti(actc + 0x14) == acta1,
		acti(actc + 0x18) == acta3, acti(acta1 + 0x18) == acta2, acti(acta1 + 0x14) == actc,
		SDTest.Call('cActClass_GetAction', 'tiis', actc, 'SDActB')[0] == acta2, SDTest.Call('cActClass_GetAction', 'tiis', actc,
		'sdactb')[0] == acta2))
	for actn in actactvars:
		check('ACT new act %s' % actn, lambda n=actn: actget(ACT_ACTVAR, acta1, n))
	# the class variables written
	for actv in (('SoundDispX', 1.5), ('SoundDispY', -2.0), ('SoundDispZ', 3.25), ('FallSoundVol', 0.75),
			('SkipInitOnChange', 1), ('DinaShip', 1), ('DinaShip', 0), ('Discardable', 0), ('CanBePossessed', 5),
			('RelaxAlarm', 1), ('FireAsSelect', 0), ('StrafeChar', 0), ('PretransformChan', 3), ('CanFly', 1),
			('CanJump', 1), ('InitEndWalkInterpFactor', 0.375), ('YModelDisp', -12.5), ('MinTimeWalking', 0.25),
			('FlyFallAccel', 98.5), ('FlySpeed', 450.0), ('FlyFric', 2.5), ('FlyAccel', 333.0), ('SRadius', 11.0),
			('VRadius', 45.0), ('WRadius', 50.0), ('MinFallDist', 29.0), ('MaxTimeToExtraRelax', 7.0),
			('MinTimeToExtraRelax', 3.0), ('TurnAngLimit', 45.0), ('CamDist', 300.0), ('CamAng', 15.0),
			('CamRot', -30.0), ('FOV', 70.0), ('MinFOV', 30.0), ('MaxFOV', 90.0), ('FOVInterpTime', 1.0),
			('Node0_3x', 12.5), ('Node2_6Y', -7.25), ('Node1_0y', 0.5), ('Node2_7x', 1.0), ('Node3_0x', 1.0),
			('Node0_3z', 1.0), ('Node0_3xx', 1.0), ('Node0x3x', 1.0), ('Missing', 1.0), ('Name', 1.0)):
		check('ACT set cls %s' % fmt(actv), lambda v=actv: actsetd(ACT_CLSVAR, actc, v[0], v[1]))
	for actv in (('FallSound', 'SDActFall'), ('OnCreate', ''), ('PossessionPress', ''), ('PhotoPress', ''),
			('ActionRelease', ''), ('ActionPress', ''), ('OnFreeResources', ''), ('DefAction', 'SDActB'),
			('Node0_Name', '_Move_The_Model_'), ('Name', 'SDActOther'), ('Missing', 'x')):
		check('ACT set cls %s' % fmt(actv), lambda v=actv: actsets(ACT_CLSVAR, actc, v[0], v[1]))
	check('ACT set cls no s', lambda: (SDTest.Call(ACT_CLSVAR, 'tiisiiii', actc, 'FallSound', 1, 0, 0, 0)[0],
		SDTest.Call(ACT_CLSVAR, 'tiisiiii', actc, 'SoundDispX', 1, 0, 0, 0)[0],
		SDTest.Call(ACT_CLSVAR, 'tiisiiii', actc, 'CanFly', 1, 0, 0, 0)[0]))
	for actn in actclsvars:
		check('ACT new cls set %s' % actn, lambda n=actn: actget(ACT_CLSVAR, actc, n))
	check('ACT cls nodes', lambda: (acti(actc + 0x5c), actfl(actc + 0x60, 14), acti(actc + 0x98), actfl(actc + 0x9c,
		14), acti(actc + 0xd4), actfl(actc + 0xd8, 14), actfl(actc + 0x110, 10)))
	check('ACT SetDefAction', lambda: (SDTest.Call('cActClass_SetDefAction', 'tiis', actc, 'SDActX')[0], acti(actc + 0x20),
		SDTest.Call('cActClass_SetDefAction', 'tiis', actc, 'SDActA')[0] == acta1, acti(actc + 0x20) == acta1))
	# the warning paths of the model and the animation (the fields go back to NULL)
	actt = acttext('SDActModel.M3D')
	actpoke(actc + 0x1c, actt)
	check('ACT SetModelFile set', lambda: (SDTest.Call('cActClass_SetModelFile', 'tvis', actc, 'SDActOther.M3D'),
		acti(actc + 0x1c) == actt, actget(ACT_CLSVAR, actc, 'ModelFilePath')))
	actpoke(actc + 0x1c, 0)
	actpoke(acta1 + 0xc, actt)
	check('ACT SetAnmFile set', lambda: (SDTest.Call('cAction_SetAnmFile', 'tvis', acta1, 'SDActOther.M3D'),
		acti(acta1 + 0xc) == actt, acti(acta1 + 0x10), actget(ACT_ACTVAR, acta1, 'AnmFilePath')))
	actpoke(acta1 + 0xc, 0)
	actfree(actt)
	# the action variables written
	for actv in (('PriorityLevel', 5), ('Chanel', 1), ('isLoop', 7), ('UseDisp', 1), ('UseFlyDisp', 0),
			('UseFlyFall', 0), ('SlideAsFly', 2), ('anmConeView', 1), ('NodeModifier', 4), ('MaxSpeed', 1.5),
			('MinSpeed', 0.5), ('RotSpeed', 90.0), ('AngRotLimit', 180.0), ('AngleModifierSpeed', 2.5),
			('AutoFaceSpeed', 30.0), ('AutoFaceOffsetH', 10.0), ('AutoFaceOffsetV', -5.0), ('InterpTime', 0.2),
			('AnmFPS', 25.0), ('AnmTotalTime', 3.0), ('TimeOfFrame1', 1.0), ('Missing', 1.0), ('EventTime', 1.0)):
		check('ACT set act %s' % fmt(actv), lambda v=actv: actsetd(ACT_ACTVAR, acta1, v[0], v[1]))
	for actv in (('AnmName', 'x'), ('Name', 'x'), ('Missing', 'x'), ('EventName', 'x')):
		check('ACT set act %s' % fmt(actv), lambda v=actv: actsets(ACT_ACTVAR, acta1, v[0], v[1]))
	for actn in actactvars:
		check('ACT new act set %s' % actn, lambda n=actn: actget(ACT_ACTVAR, acta1, n))
	actsetd(ACT_ACTVAR, acta2, 'PriorityLevel', 3)
	actsetd(ACT_ACTVAR, acta2, 'Chanel', 1)
	actsetd(ACT_ACTVAR, acta3, 'Chanel', 2)
	# the events: sorted by time, a new one before the ones of its time
	for actv in ((1.5, 'E15'), (0.5, 'E05'), (1.0, 'E10a'), (1.0, 'E10b')):
		check('ACT AddEventAtTime %s' % fmt(actv), lambda v=actv: (actsetd(ACT_ACTVAR, acta1, 'AddEventAtTime', v[0]),
			actsets(ACT_ACTVAR, acta1, 'EventName', v[1]), actevname(acti(acta1 + 0x28)), actevents(acta1)))
	check('ACT AddEvent', lambda: (SDTest.Call('cAction_AddEvent', 'tvif', acta1, 2.0), actsets(ACT_ACTVAR, acta1, 'EventName',
		'E20'), actevents(acta1)))
	for actn in ('EventName', 'EventTime', 'EventDiscardable', 'Eventname', 'EventFunc', 'EventMissing', 'Event'):
		check('ACT event get %s' % actn, lambda n=actn: actget(ACT_ACTVAR, acta1, n))
	check('ACT event set', lambda: (actsetd(ACT_ACTVAR, acta1, 'EventDiscardable', 0), actsets(ACT_ACTVAR, acta1,
		'EventFunc', ''), actsetd(ACT_ACTVAR, acta1, 'EventTime', 3.0), actsets(ACT_ACTVAR, acta1, 'EventTime', 'x'),
		actget(ACT_ACTVAR, acta1, 'EventDiscardable'), actget(ACT_ACTVAR, acta1, 'EventTime')))
	actev = acti(acta1 + 0x24)
	check('ACT event var', lambda: (actget(ACT_EVVAR, actev, 'Name'), actget(ACT_EVVAR, actev, 'Time'),
		actget(ACT_EVVAR, actev, 'Discardable'), actget(ACT_EVVAR, actev, 'Func'), actsetd(ACT_EVVAR, actev,
		'Time', 1.0), actsetd(ACT_EVVAR, actev, 'Discardable', 2), actget(ACT_EVVAR, actev, 'Discardable'),
		SDTest.Call(ACT_EVVAR, 'tiisiiii', actev, 'Name', 1, 0, 0, 0)[0], SDTest.Call(ACT_EVVAR, 'tiisiiii', actev,
		'Time', 0, 0, 0, 0)[0]))
	check('ACT Fire empty', lambda: SDTest.Call('cActEvent_Fire', 'tvis', actev, 'Player0'))
	check('ACT anm time', lambda: actsetfl(acta1 + 0x1c, (1.8,)))

	# ---- a copy of the controller of Player0 with our class: layer 0 SDActB, layer 1 SDActA, default SDActA ----
	actcc = 0
	if actpc:
		actcc = actz(0x80)
		SDTest.Call('crt_memcpy', 'iiii', actcc, actpc, 0x7c)
		actpoke(actcc + 8, actc)
		actpoke(actcc + 0xc, acta1)
		for actv in ((0, acta2, 1), (1, acta1, 1), (2, 0, 0), (3, 0, 0)):
			actpoke(actcc + 0x10 + 0x10 * actv[0], 0)
			actpoke(actcc + 0x14 + 0x10 * actv[0], actv[1])
			actpoke(actcc + 0x18 + 0x10 * actv[0], 0)
			actpoke(actcc + 0x1c + 0x10 * actv[0], actv[2])
		actpoke(actcc + 0x58, 0)
	if actcc:
		check('ACT cc names', lambda: (SDTest.Call('cActCtrl_GetActionName', 'tsii', actcc, 0)[0], SDTest.Call('cActCtrl_GetActionName', 'tsii', actcc,
			1)[0], SDTest.Call('cActCtrl_GetActionName', 'tsii', actcc, 2)[0], SDTest.Call('cActCtrl_GetDefLoopName', 'tsi', actcc)[0],
			SDTest.Call('cActCtrl_GetLayer', 'tiii', actcc, 1)[0] == actcc + 0x20))
		check('ACT cc IsActive', lambda: (SDTest.Call('cActCtrl_IsActive', 'tbis', actcc, 'SDActA')[0], SDTest.Call('cActCtrl_IsActive',
			'tbis', actcc, 'SDActB')[0], SDTest.Call('cActCtrl_IsActive', 'tbis', actcc, 'SDActC')[0], SDTest.Call('cActCtrl_IsActive',
			'tbis', actcc, 'sdacta')[0], SDTest.Call('cActCtrl_IsDefLoop', 'tbis', actcc, 'SDActA')[0], SDTest.Call('cActCtrl_IsDefLoop',
			'tbis', actcc, 'SDActB')[0]))
		for actn in ('Action', 'DefLoop', 'ActChan0', 'ActChan1', 'ActChan2', 'ActChan3', 'ActChan4', 'ForceAction',
				'Missing'):
			check('ACT cc VarS %s' % actn, lambda n=actn: (lambda r: (r[0], r[0] and actstr(r[1])))(
				SDTest.Call('cActCtrl_VarS', 'tbispi', actcc, n, 0, 0)))
		# the default loop: a lower priority is refused unless forced
		def actvars_cc(name, text):
			p = acttext(text)
			r = SDTest.Call('cActCtrl_VarS', 'tbispi', actcc, name, p, 1)[0]
			actfree(p)
			return (r, SDTest.Call('cActCtrl_GetDefLoopName', 'tsi', actcc)[0])
		for actv in (('DefLoop', 'SDActB'), ('DefLoop', 'SDActX'), ('ForceDefLoop', 'SDActX'), ('ForceDefLoop',
				'SDActB'), ('DefLoop', 'SDActA'), ('Action', 'SDActX'), ('ForceAction', 'SDActX'), ('ActChan1', 'x'),
				('Missing', 'x')):
			check('ACT cc set %s' % fmt(actv), lambda v=actv: actvars_cc(v[0], v[1]))
		for actv in (('SDActB', 0), ('SDActA', 0), ('SDActB', 1), ('SDActX', 0), ('SDActA', 0)):
			check('ACT cc SetDefLoop %s' % fmt(actv), lambda v=actv: (SDTest.Call('cActCtrl_SetDefLoop', 'tvisi', actcc, v[0],
				v[1]), SDTest.Call('cActCtrl_GetDefLoopName', 'tsi', actcc)[0]))
		# the plays that are refused before the model
		check('ACT cc PlayAction refused', lambda: (SDTest.Call('cActCtrl_PlayAction', 'tbiiiif', actcc, 0, 1, 1, 1.0)[0],
			SDTest.Call('cActCtrl_PlayAction', 'tbiiiif', actcc, acta2, 0, 1, 1.0)[0], SDTest.Call('cActCtrl_PlayActionNew', 'tbiiif', actcc,
			acta1, 1, 1.0)[0], SDTest.Call('cActCtrl_PlayActionNew', 'tbiiif', actcc, 0, 1, 1.0)[0], SDTest.Call('cActCtrl_PlayNew', 'tbisif',
			actcc, 'SDActA', 0, 1.0)[0], SDTest.Call('cActCtrl_PlayNew', 'tbisif', actcc, 'SDActX', 0, 1.0)[0],
			SDTest.Call('cActCtrl_Play', 'tbisif', actcc, 'SDActX', 0, 1.0)[0], SDTest.Call('cActCtrl_GetActionName', 'tsii', actcc, 1)[0]))
		check('ACT cc VarI read', lambda: SDTest.Call('cActCtrl_VarI', 'tbispi', actcc, 'StopSecondary', 3, 0)[0:2])
		# the events of layer 1 (SDActA ends at 1.8)
		def actlayer(i):
			return (actevname(acti(actcc + 0x18 + 0x10 * i)), acti(actcc + 0x14 + 0x10 * i) == acta1,
				actfl(actcc + 0x10 + 0x10 * i, 1))
		actpoke(actcc + 0x28, acti(acta1 + 0x24))
		for actv in ((1.2, 0), (1.2, 0), (1.6, 1), (1.7, 0), (1.9, 1), (5.0, 0)):
			check('ACT cc FireEvents %s' % fmt(actv), lambda v=actv: (SDTest.Call('cActCtrl_FireEvents', 'tviifi', actcc, 1, v[0],
				v[1]), actlayer(1)))
		actpoke(actcc + 0x28, acti(acta1 + 0x24))
		check('ACT cc FireEvents silent', lambda: (SDTest.Call('cActCtrl_FireEvents', 'tviifi', actcc, 1, 0.75, 1), actlayer(1),
			SDTest.Call('cActCtrl_FireEvents', 'tviifi', actcc, 1, -2.0, 1), actlayer(1)))
		# a layer without an action ends at -1
		actpoke(actcc + 0x38, acti(acta1 + 0x24))
		check('ACT cc FireEvents no action', lambda: (SDTest.Call('cActCtrl_FireEvents', 'tviifi', actcc, 2, -3.0, 0),
			actlayer(2), SDTest.Call('cActCtrl_FireEvents', 'tviifi', actcc, 2, -1.0, 0), actlayer(2)))
		actpoke(actcc + 0x38, 0)
		# the event functions of the engine on Player0 (only where it stays as it was)
		actdispl = actfl(actplayer + 0x468, 1)
		for actv in ((0.1, 'SDActX', '!AnmSetAction', 'Player0'), (0.2, 'SDActX', '!AnmSetDefaultLoop', 'Player0'),
				(0.3, 'SDActX:3', '!AnmChangeRandomAnm', 'Player0'), (0.35, 'SDActX:0.5', '!AnmChangeRandomAnm',
				'Player0'), (0.4, '12.5', '!AnmSetDisplAng', 'Player0'), (0.5, '3', '!AnmStopChan', 'Player0'),
				(0.6, 'SDActNoSnd:0.5:2:0.25', '!AnmSound2DEvent', 'SDActNoEnt'), (0.61, 'SDActNoSnd:0.5:2:0.25',
				'!AnmSound2DEvent', 'Player0'), (0.62, 'SDActNoSnd:0.25', '!AnmSound2DEvent', 'Player0'),
				(0.7, 'SDActNoSnd:0.5:2', '!AnmSoundEvent', 'SDActNoEnt'), (0.71, 'SDActNoSnd:0.5:2', '!AnmSoundEvent',
				'Player0'), (0.72, 'SDActNoSnd:0.5', '!AnmSoundEvent', 'Player0'), (0.8, '99:0.5:1',
				'!AnmSoundLoopEvent', 'SDActNoEnt'), (0.81, '99:0.5:1', '!AnmSoundLoopEvent', 'Player0'),
				(0.9, 'SDActNoTalk', '!StopStreamingVoice', 'SDActNoEnt')):
			SDTest.Call('crt_srand', 'vi', 77)
			check('ACT native %s' % fmt(actv), lambda v=actv: (actsetd(ACT_ACTVAR, acta3, 'AddEventAtTime', v[0]),
				actsets(ACT_ACTVAR, acta3, 'EventName', v[1]), actsets(ACT_ACTVAR, acta3, 'EventFunc', v[2]),
				SDTest.Call('cActEvent_Fire', 'tvis', acti(acta3 + 0x28), v[3]), actfl(actplayer + 0x468, 1),
				actb(actpc + 0x4c), SDTest.Call('crt_rand', 'i')[0]))
		actsetfl(actplayer + 0x468, actdispl)
		check('ACT native events', lambda: actevents(acta3))
		actfree(actcc)

	# ---- the camera of a class into a follow camera of calloc memory ----
	actcam = actz(0x240)
	actsetfl(actcam + 0xa8, (2.0,))
	check('ACT cam update', lambda: (SDTest.Call('cActClassCam_Update', 'tviii', actc + 0x110, actcam, 0), actfl(actcam + 0x214, 3),
		actfl(actcam + 0xa8, 1), SDTest.Call('cActClassCam_Update', 'tviii', actcls + 0x110, actcam, actplayer),
		actfl(actcam + 0x214, 3), actfl(actcam + 0xa8, 1)))
	# the zoom of the class "Functionary": the class of the controller of Player0 for a moment, its frame time 0.25
	actfn = 0
	if actpc and not SDTest.Call('cActClassList_Get', 'tiis', actlist, 'Functionary')[0]:
		actfn = SDTest.Call('cActClassList_Create', 'tiis', actlist, 'Functionary')[0]
	if actfn:
		for actv in (('MinFOV', 40.0), ('MaxFOV', 80.0), ('FOV', 60.0), ('FOVInterpTime', 1.0)):
			actsetd(ACT_CLSVAR, actfn, actv[0], actv[1])
		actsaved = (acti(actpc + 8), acti(actplayer + 0x504), acti(actplayer + 0x4b4), acti(actplayer + 0x11c))
		actpoke(actpc + 8, actfn)
		actsetfl(actplayer + 0x11c, (0.25,))
		# fov (rad), the zoom time left and the time it went back, the action button, the energy bar
		for actv in ((2.0, 0.0, 0.0, 0, 0.0), (1.0, 0.5, 1.0, 0, 0.0), (1.0, 0.5, 1.0, 1, 0.0), (0.5, 0.0, 0.0, 1, 0.5),
				(0.5, 0.5, 0.0, 1, 0.5), (0.5, 0.9, 0.0, 1, 0.5), (1.5, 0.9, 0.0, 1, 0.5), (0.2, 0.0, 0.0, 0, 0.5)):
			check('ACT cam zoom %s' % fmt(actv), lambda v=actv: (actsetfl(actcam + 0xa8, (v[0],)),
				actsetfl(actfn + 0x120, (v[1], v[2])), actpoke(actplayer + 0x504, (actsaved[1] & ~0xff) | v[3]),
				actsetfl(actplayer + 0x4b4, (v[4],)), SDTest.Call('cActClassCam_Update', 'tviii', actfn + 0x110, actcam,
				actplayer), actfl(actcam + 0xa8, 1), actfl(actfn + 0x120, 2)))
		actpoke(actpc + 8, actsaved[0])
		actpoke(actplayer + 0x504, actsaved[1])
		actpoke(actplayer + 0x4b4, actsaved[2])
		actpoke(actplayer + 0x11c, actsaved[3])
		check('ACT del Functionary', lambda: SDTest.Call('cActClassList_Del', 'tbis', actlist, 'Functionary')[0])
	actfree(actcam)

	# ---- an action and an event of our own (not in the class) ----
	actsa = actz(0x6c)
	check('ACT action ctor', lambda: (SDTest.Call('cAction_ctor', 'tiiis', actsa, actc, 'SDActStand')[0] == actsa,
		acti(actsa) - 0x400000, acti(actsa + 4), actstr(acti(actsa + 8)), acti(actsa + 0x14) == actc,
		actfl(actsa + 0x1c, 2), acti(actsa + 0x2c), actfl(actsa + 0x30, 1), actb(actsa + 0x34), actfl(actsa + 0x38, 4),
		(actb(actsa + 0x48), actb(actsa + 0x49), actb(actsa + 0x4a)), actfl(actsa + 0x50, 4), acti(actsa + 0x64),
		acti(actsa + 0x68)))
	check('ACT action dtor', lambda: (SDTest.Call('cAction_dtor', 'tvi', actsa), acti(actsa) - 0x400000))
	check('ACT action Delete', lambda: (SDTest.Call('cAction_ctor', 'tiiis', actsa, actc, 'SDActStand2')[0] == actsa,
		SDTest.Call('cAction_Delete', 'tiii', actsa, 0)[0] == actsa))
	actfree(actsa)
	actse = actz(0x24)
	check('ACT event ctor', lambda: (SDTest.Call('cActEvent_ctor', 'tiifi', actse, 0.75, 1234)[0] == actse, actfl(actse, 1),
		actstr(acti(actse + 4)), acti(actse + 0x1c), actb(actse + 0x20)))
	SDTest.Call('cPy_Function_dtor', 'tvi', actse + 8)
	SDTest.Call('cString_dtor', 'tvi', actse + 4)
	actfree(actse)

	check('ACT del our class', lambda: (SDTest.Call('cActClassList_Del', 'tbis', actlist, 'SDActCls')[0], SDTest.Call('cActClassList_Get',
		'tiis', actlist, 'SDActCls')[0], SDTest.Call('cActClassList_Del', 'tbis', actlist, 'SDActCls')[0]))

# ---- cAI of calloc memory on Player0 ----
actai = actz(0x38)
if actplayer:
	check('ACT AI ctor', lambda: (SDTest.Call('cAI_ctor', 'tiii', actai, actplayer)[0] == actai, acti(actai) - 0x400000,
		actb(actai + 4), acti(actai + 8) == actplayer, actfl(actai + 0xc, 1), acti(actai + 0x10) - 0x400000,
		actfl(actai + 0x14, 3), actb(actai + 0x20), acti(actai + 0x24), actb(actai + 0x28), actfl(actai + 0x2c, 2),
		acti(actai + 0x34)))
	def actaif():
		return (actb(actai + 4), actfl(actai + 0xc, 1), actb(actai + 0x28), actfl(actai + 0x2c, 2))
	actpokeb = lambda a, v: actpoke(a, (acti(a) & ~0xff) | v)
	check('ACT AI Update', lambda: (actpokeb(actai + 0x28, 1), SDTest.Call('cAI_Update', 'tvif', actai, 0.25), actaif(),
		SDTest.Call('cAI_Update', 'tvif', actai, 0.5), actaif()))
	actpoke(actai + 0x34, 1)
	check('ACT AI Update i34', lambda: (actpokeb(actai + 0x28, 1), SDTest.Call('cAI_GetChanged', 'tbi', actai)[0],
		SDTest.Call('cAI_Update', 'tvif', actai, 0.125), actaif(), SDTest.Call('cAI_GetChanged', 'tbi', actai)[0]))
	actpoke(actai + 0x34, 0)
	check('ACT AI UpdateOff', lambda: (actpokeb(actai + 4, 0), actpokeb(actai + 0x28, 1), SDTest.Call('cAI_UpdateOff', 'tvif',
		actai, 0.75), actaif()))
	check('ACT AI Vars', lambda: (SDTest.Call('cAI_VarI', 'tbispi', actai, 'Life', 3, 1)[0:2], SDTest.Call('cAI_VarF',
		'tbisDi', actai, 'Life', 2.5, 0)[0:2], SDTest.Call('cAI_VarS', 'tbispi', actai, 'Name', 0, 0)[0:2],
		SDTest.Call('cAI_VarV', 'tbisDDDi', actai, 'Pos', 1.0, 2.0, 3.0, 0)[0], SDTest.Call('cAI_Draw', 'tvi', actai)))
	check('ACT AI IsVisible', lambda: SDTest.Call('cAI_IsVisible', 'tbi', actai)[0])
	check('ACT AI dtor', lambda: (SDTest.Call('cAI_dtor', 'tvi', actai), acti(actai) - 0x400000,
		SDTest.Call('cAI_Delete', 'tiii', actai, 0)[0] == actai))
check('ACT AI ctor default', lambda: (SDTest.Call('cAI_ctorDefault', 'tii', actai)[0] == actai, acti(actai) - 0x400000,
	acti(actai + 0x10) - 0x400000, actfl(actai + 0x14, 3)))
actfree(actai)

# ---- the ranges of the bank and bishop AIs on calloc memory (the other states go to the ones of cAIChar) ----
actbk = actz(0x398)
for actv in (('cAIBankDirector_SetRanges', 2), ('cAIBankDirector_SetRanges', 3), ('cAIBankDirector_SetRanges', 7), ('cAIBankDirector_SetRanges', 1), ('cAIBankMaster_SetRanges', 2), ('cAIBankMaster_SetRanges', 3), ('cAIBankMaster_SetRanges', 7),
		('cAIBankMaster_SetRanges', 5), ('cAIBishop_SetRanges', 2), ('cAIBishop_SetRanges', 3), ('cAIBishop_SetRanges', 7), ('cAIBishop_SetRanges', 0)):
	check('ACT AI SetRanges %s' % fmt(actv), lambda v=actv: (actsetfl(actbk + 0x1b0, (-1.0, -2.0, -3.0, -4.0)),
		actsetfl(actbk + 0x394, (321.0,)), actpoke(actbk + 0x1a0, v[1]), SDTest.Call(v[0], 'tvi', actbk),
		actfl(actbk + 0x1b0, 4)))
if actplayer:
	actpoke(actbk, symva('cAIBishop_vftable'))
	actpoke(actbk + 0x40, actplayer)
	actpoke(actbk + 0x1a0, 2)
	check('ACT Bishop SetShoot', lambda: (SDTest.Call('cAIBishop_SetShoot', 'tvi', actbk), acti(actbk + 0x388), actfl(actbk + 0x38c,
		3), actfl(actbk + 0x1b0, 4)))
actfree(actbk)
