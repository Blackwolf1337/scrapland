# helpers for the ship editor cases (round 5a): the scorer, the module variables, the ships of the hangars
import Scrap, SScorer
exec(open('fpk.py').read())
_SCV = ('Text', 'X', 'Y', 'W', 'H', 'Visible', 'Alpha', 'Red', 'Green', 'Blue', 'OnAccept', 'Up', 'Down', 'Left', 'Right',
	'Hint', 'OnGainFocus', 'OnLooseFocus', 'ScaleX', 'Highlight', 'TextColorRed', 'TextColorBlue')
def _sc(pat=None, vs=None):
	# the scorer items of player 0 in draw order (those whose name contains pat), with their attributes
	vs = vs or _SCV
	r = [SScorer.GetDefault(0)]
	for n in SScorer.Items(0) or []:
		if pat and __import__("string").find(n, pat) < 0:
			continue
		row = [n]
		for v in vs:
			try:
				x = SScorer.Get(0, n, v)
				if type(x) == type(0.0):
					x = round(x, 3)
				row.append(x)
			except:
				row.append('?')
		r.append(tuple(row))
	return r
def _nm(e):
	try:
		return e and e.Name
	except:
		return 'deleted'
def _se():
	S = __import__('ShipEdit')
	H = __import__('HangarTab')
	r = []
	for v in ('prevTab', 'tabs', 'destroyedShips', 'fromSputnik', 'auxShipsToEditList', 'shipsToEditList', 'lastCam',
		'ctnRotating', 'isRotating', 'tabPixelX', 'editShipPosDummy'):
		r.append((v, getattr(S, v, '?')))
	r.append(('editedShips', map(_nm, S.editedShips)))
	r.append(('camera', _nm(S.camera)))
	r.append(('editPlatform', _nm(S.editPlatform)))
	for v in ('prevItem', 'prevHangar', 'currentHangar', 'currentShipToEdit', 'currentPilot', 'holograms', 'shipTotalCost'):
		r.append((v, getattr(H, v, '?')))
	r.append(('prevShip', _nm(H.prevShip)))
	r.append(('miniShip', _nm(H.miniShip)))
	return r
_EDV = ('Model', 'Active', 'ActiveCMask', 'MaxLife', 'MinLife', 'MEng0', 'MEng1', 'MEng2', 'MEng3', 'NEng0', 'NEng1',
	'NEng2', 'NEng3', 'WeapList', 'Upgrd00', 'Upgrd01', 'Upgrd02', 'Upgrd03', 'Upgrd04', 'Upgrd05', 'UAmmo00', 'UAmmo03',
	'LinkModel', 'OffModPos', 'ArrowCanUse', 'MoveTime', 'OnEndMove', 'RenderEffect', 'MaxSpeed', 'BoostTime', 'AcMass')
def _ships():
	S = __import__('ShipEdit')
	H = __import__('HangarTab')
	r = []
	for e in S.editedShips + [H.prevShip]:
		if e:
			try:
				r.append(_v(e.Name, _EDV))
			except:
				r.append('deleted')
		else:
			r.append(None)
	return r
def _hv():
	return map(lambda v: (v, Scrap.GetSaveVar(v)), ('Hangar.HangarShip1', 'Hangar.HangarShip2', 'Hangar.HangarShip3',
		'Hangar.HangarShip4', 'Hangar.HangarShip5', 'Hangar.HangarShip6', 'Hangar.HangarShip7', 'Hangar.HangarShip8',
		'Hangar.HangarShip9', 'Hangar.HangarShipAux', 'Hangar.DestroyedShips', 'Hangar.iHangarShip'))
def _gvs():
	r = []
	for v in ('HangarShip1', 'HangarShip2', 'HangarShip3', 'HangarShip4', 'HangarShip5', 'HangarShip6', 'HangarShip7',
		'HangarShip8', 'HangarShip9', 'iHangarShip', 'PlayerName', 'PilotModel', 'PlayerMaxLife', 'PlayerModel',
		'Motor0Model', 'Motor1Model', 'Motor2Model', 'Motor3Model', 'WeaponBayList', 'VehicleEditingMode'):
		try:
			r.append((v, Scrap.Get(v)))
		except:
			r.append((v, '?'))
	return r
_SETYPES = {'BackgroundElementL': 'Sprite', 'BackgroundElementR': 'Sprite', 'BackgroundElementU1': 'Sprite', 'BackgroundElementU2': 'Sprite', 'BackgroundElementD': 'Sprite', 'HangarShip': 'HangarShip', 'MiniMaxSpeedText': 'Text', 'MiniMaxSpeed': 'Text', 'MiniSpeedRamp1': 'Sprite', 'MiniSpeedRamp2': 'Sprite', 'MiniBoostRechargeTimeText': 'Text', 'MiniBoostRechargeTime': 'Text', 'MiniHullText': 'Text', 'MiniHullValue': 'Text', 'MiniWeight': 'Text', 'MiniCurrentWeight': 'Text', 'MiniMaxWeight': 'Text', 'MiniSlashWeight': 'Text', 'MiniShipInfo': 'Button', 'HLine': 'Sprite', 'VLine': 'Sprite', 'MLine': 'Sprite', 'Hint': 'Hint', 'nAux': 'Text', 'LittleShipAux': 'Button', 'HangarAux': 'Button', 'AuxShipText': 'Text', 'VulcanSpr': 'Sprite', 'DevastatorSpr': 'Sprite', 'TeslaSpr': 'Sprite', 'ATPCSpr': 'Sprite', 'SwarmSpr': 'Sprite', 'InfernoSpr': 'Sprite', 'NumVulcans': 'Text', 'NumDevastators': 'Text', 'NumTeslas': 'Text', 'NumATPCs': 'Text', 'NumSwarmMissiles': 'Text', 'NumInfernoMissiles': 'Text', 'ProjectilesAmmo': 'Sprite', 'EnergyAmmo': 'Sprite', 'MissilesAmmo': 'Sprite', 'NumSlotsParticleWeapons': 'Text', 'NumSlotsEnergyWeapons': 'Text', 'NumSlotsMissileWeapons': 'Text', 'HullTextShowInfo': 'Text', 'Hull': 'Text', 'Weight': 'Text', 'CurrentWeight': 'Text', 'MaxWeight': 'Text', 'Slash': 'Text', 'MaxSpeed': 'Text', 'MaxSpeedText': 'Text', 'BoostRechargeTimeText': 'Text', 'BoostRechargeTime': 'Text', 'BoostUp': 'Sprite', 'BoostDown': 'Sprite', 'BoostEqual': 'Sprite', 'BoostBack': 'Sprite', 'SpeedUp': 'Sprite', 'SpeedDown': 'Sprite', 'SpeedEqual': 'Sprite', 'SpeedBack': 'Sprite', 'WeightUp': 'Sprite', 'WeightDown': 'Sprite', 'WeightEqual': 'Sprite', 'WeightBack': 'Sprite', 'SpeedRamp1': 'Sprite', 'SpeedRamp2': 'Sprite', 'ShipInfoML': 'Sprite', 'ShipInfoMLCircuit': 'Sprite', 'ShipInfoMR': 'Sprite', 'ShipInfoMRCircuit': 'Sprite', 'ShipInfo': 'Sprite', 'MoneyIcon': 'Sprite', 'Money': 'Text', 'MoneyTab': 'Tab', 'MoneyPredict': 'Text', 'MoneyPredictTab': 'Tab', 'Sell': 'Button', 'SellBack': 'Sprite', 'SellCircuit1': 'Sprite', 'SellCircuit2': 'Sprite', 'SellCircuit3': 'Sprite', 'SellCircuit4': 'Sprite', 'Exit': 'Button', 'ExitBack': 'Sprite', 'ExitCircuit1': 'Sprite', 'ExitCircuit2': 'Sprite', 'ExitCircuit3': 'Sprite', 'ExitCircuit4': 'Sprite', 'PilotRect': 'Sprite', 'PrevPilot': 'Button', 'PrevPilotBack': 'Sprite', 'PrevPilotCircuit1': 'Sprite', 'PrevPilotCircuit2': 'Sprite', 'PrevPilotCircuit3': 'Sprite', 'NextPilot': 'Button', 'NextPilotBack': 'Sprite', 'NextPilotCircuit1': 'Sprite', 'NextPilotCircuit2': 'Sprite', 'NextPilotCircuit3': 'Sprite', 'PilotText': 'Text', 'PilotTextRect': 'Sprite', 'PilotName': 'Text', 'PilotNameTab': 'Tab', 'ShipType': 'Button', 'ShipTypeName': 'Text', 'ShipTypeBack': 'Sprite', 'ShipTypeCircuit1': 'Sprite', 'ShipTypeCircuit2': 'Sprite', 'PrevShip': 'Button', 'PrevShipBack': 'Sprite', 'PrevShipCircuit1': 'Sprite', 'PrevShipCircuit2': 'Sprite', 'PrevShipCircuit3': 'Sprite', 'NextShip': 'Button', 'NextShipBack': 'Sprite', 'NextShipCircuit1': 'Sprite', 'NextShipCircuit2': 'Sprite', 'NextShipCircuit3': 'Sprite', 'n1': 'Text', 'LittleShip1': 'Button', 'Hangar1': 'Button', 'n2': 'Text', 'LittleShip2': 'Button', 'Hangar2': 'Button', 'n3': 'Text', 'LittleShip3': 'Button', 'Hangar3': 'Button', 'n4': 'Text', 'LittleShip4': 'Button', 'Hangar4': 'Button', 'n5': 'Text', 'LittleShip5': 'Button', 'Hangar5': 'Button', 'n6': 'Text', 'LittleShip6': 'Button', 'Hangar6': 'Button', 'n7': 'Text', 'LittleShip7': 'Button', 'Hangar7': 'Button', 'n8': 'Text', 'LittleShip8': 'Button', 'Hangar8': 'Button', 'n9': 'Text', 'LittleShip9': 'Button', 'Hangar9': 'Button', 'ShipEditTabHangar': 'Text', 'Tab_ShipEditTabHangar': 'Tab', 'ShipEditTabEngines': 'Text', 'Tab_ShipEditTabEngines': 'Tab', 'ShipEditTabWeapons': 'Text', 'Tab_ShipEditTabWeapons': 'Tab', 'ShipEditTabHull': 'Text', 'Tab_ShipEditTabHull': 'Tab'}
_SEV2 = {'Text': ('Align', 'Effect', 'MaxInput', 'CenterEdit', 'EditHint', 'TextColorAlpha', 'TextColorGreen'),
	'Sprite': ('Rotate', 'Mirror', 'Unit', 'PivotX', 'PivotY', 'Discardable', 'IsMultiSprite'),
	'Button': ('FocusSpriteIndex', 'Complex', 'FocusPivotX', 'FocusPivotY', 'Discardable', 'IsMultiSprite', 'HighLight'),
	'Tab': ('Type', 'TabInit', 'TabEnd', 'TabMax', 'SizeX', 'SizeY', 'SizeTabQuad', 'Filled'),
	'Head': ('HeadModel', 'HeadAnimIndex', 'VW', 'VH'),
	'Hint': ('Align', 'TabSprite', 'TabSpriteIndex', 'TabAlpha', 'TabRed', 'TabGreen', 'TabBlue'),
	'HangarShip': ('CamDist', 'CamFov', 'TargetName')}
def _sc2():
	# more attributes of the editor items, by their type (getters that check their value or read unset text are left out)
	r = []
	S = __import__('string')
	for n in SScorer.Items(0) or []:
		t = _SETYPES.get(n)
		if not t:
			for p, k in (('UncompletedShipText_', 'Text'), ('DestroyedShipText_', 'Text'), ('AvailableHangarText_', 'Text'), ('P_', 'Head')):
				if n[:len(p)] == p:
					t = k
		if not t:
			continue
		row = [n]
		for v in _SEV2[t]:
			try:
				x = SScorer.Get(0, n, v)
				if type(x) == type(0.0):
					x = round(x, 3)
				row.append(x)
			except:
				row.append('?')
		r.append(tuple(row))
	return r
