##############################################################################################################
###                                          Quick console commands                                        ###
##############################################################################################################

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import SVec
import sys
import math
import SAI
import SFX

# Changes the speed of the timer
def speed(ts):
	Scrap.SetTimeSpeed(ts)

# Pause the game
LastTimeSpeed = 1.0
def stop():
	global LastTimeSpeed
	
	if LastTimeSpeed > 0:
		LastTimeSpeed = Scrap.GetTimeSpeed()
		Scrap.SetTimeSpeed(0)

# continues the game
def go():
	global LastTimeSpeed
	Scrap.SetTimeSpeed(LastTimeSpeed)
	
# activate the free camera
LastCameraTarget = "Player1"


def free():
	global FreeCam
	global LastCameraTarget
	
	SScorer.SetConsole(0)
	
	cam = Scrap.GetEntity(Scrap.GetCam(0))
	if cam.FreeCam:
		cam.FreeCam = 0
		cam.Target = LastCameraTarget
		Scrap.Set("FreeCameraCollide",1)
		Scrap.GetEntity(cam.MainTarget).UsrControl = 0
		SInput.Bind(0,"Walk","NextAction","")
	else:
		LastCameraTarget =  cam.Target 
		cam.FreeCam = 1
		Scrap.Set("FreeCameraCollide",0)
		Scrap.GetEntity(cam.MainTarget).UsrControl = 1
		
		SInput.Bind(0,"Walk","NextAction","QuickConsole.setchar")

def Free(All=0):
	global FreeCam
	global LastCameraTarget
	
	SScorer.SetConsole(0)
	
	cam = Scrap.GetEntity(Scrap.GetCam(0))
	if cam.Target == "":
		cam.Target = LastCameraTarget
		cam.FreeCam = 0
		Scrap.Set("FreeCameraCollide",1)
	else:
		LastCameraTarget =  cam.Target 
		cam.Target = ""
		Scrap.Set("FreeCameraCollide",0)
		if All:
			cam.FreeCam = 1

def act(EntityName=None,ActName="Relax"):
	if not EntityName:
		cam = Scrap.GetEntity(Scrap.GetCam(0))
		e   = Scrap.GetEntity(cam.Target)
	else:
		e = Scrap.GetEntity(EntityName)
	e.ForceAction = ActName

# te convierte en Baco, no en Zeus, Yave, Ala ni nada de eso
def godcall(Entity=None,Modes=(1,1,1,1,1,1)):
	import Init
	if Init.MultiPlayer:
		return
	
	Scrap.AddScheduledFunc(Scrap.GetTime()+0.25,godcall,(Entity,Modes))
	if Entity:
		e   = Scrap.GetEntity(Entity)
	else:
		e   = Scrap.UsrEntity(0)
	if e:
		if e.IsType("Car"):
			if Modes[1]:
				e.Ammo00     = SWeap.GetFAmmo(0,"Max")
				e.Ammo01     = SWeap.GetFAmmo(1,"Max")
				e.Ammo02     = SWeap.GetFAmmo(2,"Max")
				e.Ammo03     = SWeap.GetFAmmo(3,"Max")
			if Modes[2]:
				e.WeapList   = "63,63,63,63,63,63,1"
				e.MaxLife = e.MinLife
				if e.Life > e.MaxLife:
					e.Life = e.MaxLife
			if Modes[3]:
				e.CMStamp    = 0
				e.FireStamp  = 0
			if Modes[4]:
				e.BoostTime  = 0
			if Modes[5]:
				t = Scrap.GetEntity(e.Target)
				if t:
					t.Life = 0
		elif e.IsType("WalkChar"):
			if Modes[1]:
				e.Energy = 1
		
		if Modes[0]:
			e.Invulnerable = 1

def god(Entity=None):
	godcall(Entity,(1,1,1,0,1,0))

def evil(Entity=None):
	godcall(Entity,(1,0,0,0,0,0))

def killer(Entity=None):
	godcall(Entity,(0,1,1,0,0,0))

def yoda(Entity=None):
	godcall(Entity,(0,0,0,0,0,1))

def terminal(Entity=None):
	godcall(Entity,(0,0,0,0,1,0))

def regod(Entity=None):
	godcall(Entity,(1,1,1,1,1,0))

def megagod(Entity=None):
	godcall(Entity,(1,1,1,1,1,1))
	
def cshow(f):
	Scrap.Set("cshow",f)

def listmodels():
	Scrap.ListModels()

def modelinfo(Ename="",Nmodel=1):
	Scrap.ModelInfo(Ename,Nmodel)

def screenshot(name="Shot"):
	Scrap.ScreenShot(name+"*")

def reset3d():
	Scrap.SetVideoCurrentMode(Scrap.GetVideoCurrentMode())

def unpackdvf(name="Movie/Movie"):
	stopmovie(name)
	Scrap.ProcessDVF(name+".dvf",2)

def MakeShipAI(TypeShip = "SPoli1", Name="PlayerJoker", TypeEngine = "MPoli1", FactorDamageGetAway = 0.1, ControlStrafe = 2, ControlBrake = 0, ControlBoost = 1, StupidPathFinding = 0, MaxTimeStrafe = 4, MaxVelStrafe = 0.5, ControlAim = 3, CadenceShoot = 4, AngAttack = 0.785, FactorDamageItemLife = 0.25):
	import Vehicles
	global PlayerPos
	global PlayerAng
	global OffsetPos
	global OffsetPosNorm
	global Ang

	c = Scrap.GetEntity(Scrap.GetEntity(Scrap.GetCam(0)).Target)
	OffsetPos = 0,0,-1
	OffsetPos = SVec.Rotate3D(OffsetPos,c.Ang)
	OffsetPosNorm = SVec.Norm(OffsetPos)
	OffsetPos = SVec.Prod(OffsetPosNorm,1000)
	PlayerPos = SVec.Add(c.Pos,OffsetPos)

	PlayerAng = OffsetPosNorm
	Ang = SVec.GetRotAng(PlayerAng[0],PlayerAng[1],PlayerAng[2])
	PlayerAng = Ang[0],Ang[1],0
	PlayerAng = SVec.NormAng(PlayerAng[0]),SVec.NormAng(PlayerAng[1]),0

	o1            = Scrap.CreateEntity(Name, PlayerPos[0], PlayerPos[1], PlayerPos[2], "Car")
	Vehicles.SetStdProperties(o1.Name, TypeShip,"P_Police")
	o1.Descriptor = "Assasin"
	o1.TgType     = "killer"
	o1.Ang        = PlayerAng
	SAI.EnableAIVehicle(o1.Name, 1, FactorDamageGetAway, ControlStrafe, ControlBrake, ControlBoost, StupidPathFinding, MaxTimeStrafe, MaxVelStrafe, ControlAim, CadenceShoot, AngAttack, FactorDamageItemLife)

#	o1.AI_OnPrueba = "Automat.OnPrueba"
#	o1.AI_OnPrueba = "Vehicles.OnDeath"

def MakeCharAI(TypeChar = "Sentinel", StupidPathFinding = 0, Name="PlayerJoker"):
	global PlayerPos
	global PlayerAng
	global OffsetPos
	global OffsetPosNorm
	global Ang

	c = Scrap.GetEntity(Scrap.GetEntity(Scrap.GetCam(0)).Target)
	OffsetPos = 0,0,-1
	OffsetPos = SVec.Rotate3D(OffsetPos,c.Ang)
	OffsetPosNorm = SVec.Norm(OffsetPos)
	OffsetPos = SVec.Prod(OffsetPosNorm,300)
	PlayerPos = SVec.Add(c.Pos,OffsetPos)

	PlayerAng = OffsetPosNorm
	Ang = SVec.GetRotAng(PlayerAng[0],PlayerAng[1],PlayerAng[2])
	PlayerAng = Ang[0],Ang[1],0
	PlayerAng = SVec.NormAng(PlayerAng[0]),SVec.NormAng(PlayerAng[1]),0

	o1            = Scrap.CreateEntity(Name, PlayerPos[0], PlayerPos[1], PlayerPos[2], "WalkChar")
	o1.ActCtrl    = TypeChar
	o1.Ang        = PlayerAng
	o1.UsrControl  = -1
	SAI.EnableAIChar(o1.Name, 1, StupidPathFinding)

#	o1.AI_OnPrueba = "Automat.OnPrueba"
#	o1.AI_OnPrueba = "Vehicles.OnDeath"

def CreateGraphAI(DimNodes = 0, VectInterNodX = 1200, VectInterNodY = 1200, VectInterNodZ = 1200):
	import Viewer
	
	Viewer.More()
	SAI.BuildGraph(DimNodes, VectInterNodX, VectInterNodY, VectInterNodZ)

def CreateGraph2DAI(DimNodes = 0, VectInterNodX = 155, VectInterNodY = 155, VectInterNodZ = 155):
	import Viewer
	
	Viewer.More()
	SAI.BuildGraph2D(DimNodes, VectInterNodX, VectInterNodY, VectInterNodZ)

def AnalizeMapAI(DimNode):	
	SAI.AnalizeMap(DimNode)

def AnalizeGraphAI():
	SAI.AnalizeGraph()

def AnalizeGraph2DAI():
	SAI.AnalizeGraph2D()

def AnalizeTraffic():
	SAI.AnalizeTraffic()

def helplib(Module = "Scrap"):
	print "======== [ "+Module+" ] ========"
	for v in dir(eval(Module)):
		if v in ("__doc__","__name__"):
			continue
		print v+":"
		print "      "+eval(Module+"."+v+".__doc__")

def helpfunc(func):
	print func.__name__+":"
	print "    "+func.__doc__
	
def char(c=None):
	import SaveGame
	if c:
		Scrap.SetSaveVar("Char",c)
		SaveGame.Restart()		
	else:
		print Scrap.GetSaveVar("Char")

def load(i=0):
	import SaveGame
	SaveGame.LoadGame(i)
	
def save(i=0):
	import SaveGame
	SaveGame.SaveGame(i)
	
def PosIni():
	Scrap.GetEntity("Player1").Pos = (-7455.97070313, -43068.125, -137096.5)
	CreatePathAI(-7455.97070313, -43068.125, -137096.5, 1000, "")

def PosIni2():
	Scrap.GetEntity("Player1").Pos = (15495.6933594, -29181.4238281, 50639.96875)
	CreatePathAI(15495.6933594, -29181.4238281, 50639.96875, 1000, "")

def dbsave():
	Scrap.SetSaveVar("DebugSave","1")

def reload():
	import SaveGame,Init,string
	lang = Scrap.Get("SelectedLanguage")
	Scrap.SetLanguage(string.upper(lang))
	Scrap.SetLanguage(lang)
	SaveGame.Restart()

# modo depuracion
DebugObj = None
def dbg():
	global DebugObj
	import sys
	if not ('Tools/PyDebug' in sys.path):
		sys.path.append('Tools/PyDebug')
	import pdb	
	
	if DebugObj==None:
		d = pdb.Pdb()
		print "---[Debug mode activated]---"
	d.set_trace()


def setchar(id=0):
	Scrap.GetEntity(LastCameraTarget).Pos = Scrap.GetEntity(Scrap.GetCam(0)).camPos

def fight():
	name = Scrap.UsrEntity(0).Name
	SAI.SetStateVehicle(2, name, "")

def m(levelname):
	import SaveGame
	SaveGame.QLoad("Levels/"+levelname)

def CamEnt():
	e = Scrap.GetEntity((Scrap.GetCam(0)))
	print "Entity '"+e.Target+"' at camera '"+e.Name+"'"
	
	
# funcioncitas para lanzar una escena /play "Decontamination", 10
def play(SceneName,jump=0):
	import Scene
	Scene.Play(SceneName)
	if jump>0:
		Scrap.SetTimeSpeed(6.0)
		Scrap.AddScheduledFunc(Scrap.GetTime()+jump,Scrap.SetTimeSpeed,(1.0,))
		

def PauseGame(id):
	if Scrap.GetTimeSpeed()>0.5:
		Scrap.SetTimeSpeed(0)
	else:
		Scrap.SetTimeSpeed(1)

def ssmode():
	Scrap.Set("QChatEvent5","QuickConsole.PauseGame")
	
	id = 0
	objName = "SpeedShow"
	SScorer.Add(id,objName,"Text")
	SScorer.Set(id,objName,"X",580)
	SScorer.Set(id,objName,"Y",150)	
	SScorer.Set(id,objName,"Font","BigNumbers")	
	SScorer.Set(id,objName,"Text","\nSpeedKmH")
	SScorer.Set(id,objName,"Align","Right")
	SScorer.Set(id,objName,"Red",   255)
	SScorer.Set(id,objName,"Green", 255)
	SScorer.Set(id,objName,"Blue",  255)

StopMovie = 0
def movie(Speed = 0.25,MovieName="Movie/Movie",LastTime=None):
	global StopMovie

	if LastTime==None:
		Scrap.Set( "PhysicalAspectRatio", 16.0/9.0 )
		Scrap.OpenPack( "Sounds/Generic/Misc" )
		SSound.LoadSound( "MovieSync.wav" )
		Scrap.ClosePack()
		SSound.Play( "MovieSync" )
		LastTime = Scrap.GetTime()
		StopMovie = 0

	if StopMovie == 1:
		return

	Scrap.SetTimeSpeed(Speed)
	Scrap.Set("S_PitchScale",Speed)
	Scrap.Set("DisableSkipSlot",1)
	Scrap.ProcessDVF(MovieName+".dvf",0)
	Scrap.ScreenShot(MovieName+"*")
	NextTime = LastTime+1.0/25.0
	Scrap.AddScheduledFunc(NextTime,movie,(Speed,MovieName,NextTime))

def stopmovie(name="Movie/Movie"):
	global StopMovie
	SSound.Play("MovieSync")
	Scrap.ProcessDVF(name+".dvf",3)
	Scrap.SetTimeSpeed(1)
	Scrap.Set("S_PitchScale",1)
	Scrap.Set("DisableSkipSlot",0)
	Scrap.Set("R_LimitFPS",0)
	Scrap.Set("R_ForceFPS",0)
	Scrap.Set("AutoScreenShot",0)
	Scrap.Set("PhysicalAspectRatio",4.0/3.0)
	StopMovie = 1

def movie2(fps=30,Speed=1.0,MovieName="Movie/Movie"):
	Scrap.Set("MovieCacheSize",512)
	Speed = Speed/Scrap.Get("TimerFreqMult")
	print "Recording DVF file at",fps,"fps,",Speed,"speed."
	print "TimerFreqMult:", Scrap.Get("TimerFreqMult"), " ( should be ", 1/Speed,")"
	print "type /unpackdvf when finished"
	Scrap.Set("DisableSkipSlot",1)
	SSound.LoadSound( "Sounds/Generic/Misc/MovieSync.wav" )
	SSound.Play( "MovieSync" )
	Scrap.Set("R_LimitFPS",Speed*fps)
	# Scrap.Set("R_ForceFPS",fps)
	# Scrap.Set("MovieUseMemory",1)
	# Scrap.Set("PhysicalAspectRatio", 16.0/9.0)
	Scrap.Set("AutoScreenShot",1)
	Scrap.ProcessDVF(MovieName+".dvf",0)
	Scrap.ScreenShot(MovieName)

def moviesound():
	SSound.Play( "MovieSync" )

def movie3(fps=15,Speed=1.0,MovieName="_Movie_"):
	Speed = Speed/Scrap.Get("TimerFreqMult")
	print "Recording AVI file at",fps,"fps,",Speed,"speed."
	print "TimerFreqMult:", Scrap.Get("TimerFreqMult"), " ( should be ", 1/Speed,")"
	print "type /unpackdvf when finished"
	## Scrap.SetTimeSpeed(Speed)  #### usar: TimerFreqMult 2
	Scrap.Set("MovieFramerate",fps)
	## Scrap.Set("S_PitchScale",Speed)
	Scrap.Set("DisableSkipSlot",1)
	SSound.LoadSound( "Sounds/Generic/Misc/MovieSync.wav" )
	Scrap.Set("R_LimitFPS",Speed*fps)
	Scrap.Set("AutoScreenShot",1)
	Scrap.ProcessDVF(MovieName+".avi",0)
	Scrap.ScreenShot(MovieName)
	Scrap.AddScheduledFunc(0,moviesound,())

def camrot(angulus = 180):
	Scrap.GetEntity((Scrap.GetCam(0))).AddTurn = angulus

def join(address="Localhost",port=0):
	SNet.InitClient(address,port)

# modo "quiero ver personajes de prueba"
def m3d():
	char("TestC")

def nextdeal():
	Scrap.SetSaveVar(Scrap.GetSaveVar('CrazyDeal.1.Var'),Scrap.GetSaveVar('CrazyDeal.1.Tgt'))
	Scrap.SetSaveVar(Scrap.GetSaveVar('CrazyDeal.2.Var'),Scrap.GetSaveVar('CrazyDeal.2.Tgt'))
	Scrap.SetSaveVar(Scrap.GetSaveVar('CrazyDeal.3.Var'),Scrap.GetSaveVar('CrazyDeal.3.Tgt'))
	
def shent():
	import string
	
	eList = []
	e1 = Scrap.GetFirst()
	while e1:
		eList.append((e1.Name, e1.Type))
		e1 = Scrap.GetEntity(e1.NextInSlot)

	eList.sort()
	for e in eList:
		e1 = Scrap.GetEntity(e[0])
		
		print e1.Active,string.ljust(e[1],10) + string.ljust(e[0],24) + string.rjust(`int(e1.Pos[0])`,8)+ string.rjust(`int(e1.Pos[1])`,8)+ string.rjust(`int(e1.Pos[2])`,8)

		
def shlst(ListName):
	
	e1 = Scrap.EntityListGet   (ListName,"First")
	
	while e1:
		print e1
		e1 = Scrap.GetEntity(e1).NextInList

def antes():
	import TestC
	
	SAct.GetClass(Scrap.UsrEntity(0).ActCtrl)
	SAct.SetCls("ActionPress",  "TestC.ActionPress"  )
	SAct.SetCls("ActionRelease","TestC.ActionRelease")
	SAct.SetCls("Possession",   "TestC.SwitchAction" )


def rdr(siz = 2048):
	SScorer.Set(0,"Radar","Map2dScale",siz)
	Scrap.Set("RadarShowAll",1)
	
def newshere():
	import Init
	Scrap.SetSaveVar("Journalist.Humphrey_Defaut", Init.Path[7:])
	Scrap.SetSaveVar("Journalist.Betty_Defaut",    Init.Path[7:])
	Scrap.SetSaveVar("Journalist.Berto_Defaut",    Init.Path[7:])
	
	
def merc(i):
	import VehiclesNPC
	merc = VehiclesNPC.Mercs["SMERC"+`i`]
	if Scrap.GetSaveVar(merc) == "1":
		v = "0"
		print merc,"Inactive"
	else:
		v = "1"
		print merc,"Activated"
	
	Scrap.SetSaveVar(merc,v)
	
def mypos(x=None,y=None,z=None):
	if x!=None and y!=None and z!=None:
		Scrap.UsrEntity(0).Pos = x,y,z
	else:	
		print "e.Pos = ", Scrap.UsrEntity(0).Pos
		print "e.Ang = ", Scrap.UsrEntity(0).Ang

def PrintTheElementInfo(Name,x,y,z,angx,angy):
	print Name,"(",x,y,z,")","(",angx,angy,")"
	
def shalom():
	Scrap.CallElements("QuickConsole.PrintTheElementInfo")

def freeze():
	import string
	
	e1 = Scrap.GetFirst()
	while e1:
		if e1.IsType("WithLife") and e1.Name != Scrap.UsrEntity(0).Name:
			e1.TimeSpeed = 0		
		e1 = Scrap.GetEntity(e1.NextInSlot)

def unfreeze():
	import string
	
	e1 = Scrap.GetFirst()
	while e1:
		if e1.IsType("WithLife") and e1.Name != Scrap.UsrEntity(0).Name:
			e1.TimeSpeed = 1
		e1 = Scrap.GetEntity(e1.NextInSlot)

# imprime un dummy	
def dmm(DummyName="DummyName"):
	Scrap.ConsoleError("\n\n["+DummyName+"]\n")
	usr = Scrap.UsrEntity(0)
	if usr:
		usrpos = usr.Pos
		usrrot = usr.Ang
	else:
		cam = Scrap.GetEntity(Scrap.GetCam(0))
		usrpos = cam.camPos
		usrrot = cam.camAng
	Scrap.ConsoleError("Pos = \t"+`int( usrpos[0])`+"\t"+`int(usrpos[1]    )`+"\t"+`int(usrpos[2])`+"\n")
	Scrap.ConsoleError("Rot = \t"+`int(-usrrot[0])`+"\t"+`int(usrrot[1]+180)`+"\t"+`int(usrrot[2])`+"\n\n\n")

# Carga una mission
def mss(MissionName="MapCrazyMayor", superDealType = None):
	import MissionsFuncs,Init,SaveGame
	
	Scrap.DelSaveVars("")
	Scrap.CreateSaveVar("GameplayTime","0")
	Scrap.CreateSaveVar("Spawn",   "DM_Player_Spawn_Beginning")   # indica el dummy por defecto
	Scrap.CreateSaveVar("Char",    "Dtritus")                     # el personaje por defecto
	Scrap.CreateSaveVar("ComeFrom","")                            # Objeto que utiliza para llegar

	Scrap.CreateSaveVar('Player.NumLives',      '10')
	Scrap.CreateSaveVar('Player.InfiniteLives',  '0')
	Scrap.CreateSaveVar('Money',                 '1000000')
	Scrap.CreateSaveVar('Police.FicusDeath',     '1')

	Scrap.CreateSaveVar('Hangar.iHangarShip', '8')
	Scrap.CreateSaveVar('Hangar.availableEnginesList', "['MPOLI4', 'MPOLI5', 'MPOLIBOSS1', 'MPOLI2', 'MBERTO1', 'MBETTY1', 'MPOLI1', 'MMERC1','MMERC2', 'MMERC3', 'MPOLI3', 'MMAYOR1', 'MFUNC1', 'MBANKER1','MBANKMASTER1', 'MBISHOP1','MARCHBISHOP1', 'MHUMP1', 'MBOSS1']")
	Scrap.CreateSaveVar('Hangar.availableUpgradesList', '["VulcanUpgrade1", "VulcanUpgrade2", "DevastatorUpgrade1", "DevastatorUpgrade2", "SwarmUpgrade1", "SwarmUpgrade2", "InfernoUpgrade1", "InfernoUpgrade2", "TeslaUpgrade1", "TeslaUpgrade2", "ATPCUpgrade1", "ATPCUpgrade2"]')
	Scrap.CreateSaveVar('Hangar.availableWeaponsList', '["Vulcan", "Devastator", "Swarm","Inferno", "Tesla", "ATPC"]')
	Scrap.CreateSaveVar('Hangar.shipsToEditList', '["SPoli1", "SPoli2", "SPoli3", "SPoli4", "SPoli5", "SPoliBoss1", "SMerc1","SMerc2","SMerc3","SMayor1","SBanker1","SBankMaster1","SBishop1","SArchbishop1","SFunc1","SBerto1","SBetty1","SHump1","SBoss1"]')
	Scrap.CreateSaveVar('Hangar.DestroyedShips', '[0,0,0,0,0,0,0,0,0,0]')
	Scrap.CreateSaveVar('Hangar.HangarShipAux', 'SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0')
	Scrap.CreateSaveVar('Hangar.HangarShip9', 'SPoli1<-MPoli1<-<-<-<-1,1,1,1,1,0,1<-50<-0,0,0,0,0,0')
	Scrap.CreateSaveVar('Hangar.HangarShip8', '')
	Scrap.CreateSaveVar('Hangar.HangarShip7', '')
	Scrap.CreateSaveVar('Hangar.HangarShip6', '')
	Scrap.CreateSaveVar('Hangar.HangarShip5', '')
	Scrap.CreateSaveVar('Hangar.HangarShip4', '')
	Scrap.CreateSaveVar('Hangar.HangarShip3', '')
	Scrap.CreateSaveVar('Hangar.HangarShip2', '')
	Scrap.CreateSaveVar('Hangar.HangarShip1', '')
	Scrap.CreateSaveVar('StdShipAIProfile',   'Pilot')

	if (superDealType):
		Scrap.CreateSaveVar("SuperDealType", superDealType)
		
	MissionsFuncs.SetNextMission(MissionName,Init.Path[7:])
	Scrap.Set("ForceReload",0)
	SaveGame.Restart()

def split(mapname = "GDB",GameType=None ):
	import SaveGame
	
	if GameType:
		Scrap.Get("ServerType",GameType)
	Scrap.Set("r_SplitScreen",1)
	Scrap.Set("SplitScreen",1)
	Scrap.Set("CameraDefViewAspect",2.6666)
	Scrap.Set("R_SceneRTargetSwap",0)
	SaveGame.QLoad("Levels/"+mapname)
	

def dms(name):
	print "Searching '"+name+"*'..."
	Scrap.StartDummySearch(name+"*",1)
	lista = [] 	
	while 1:
		dummy = Scrap.NextDummySearch()
		if dummy:
			lista.append(dummy)
		else:
			break
	lista.sort()
	for f in lista:
		print f

def setcam(pos,ang,fov):	
	Scrap.GetEntity(Scrap.GetCam(0)).camPos = pos
	Scrap.GetEntity(Scrap.GetCam(0)).camAng = ang
	Scrap.GetEntity(Scrap.GetCam(0)).Fov    = fov

def getcam():	
	print "/setcam",Scrap.GetEntity(Scrap.GetCam(0)).camPos,",",Scrap.GetEntity(Scrap.GetCam(0)).camAng,",",Scrap.GetEntity(Scrap.GetCam(0)).Fov

def hidemap():
	Scrap.Set("RenderSky",0)
	Scrap.Set("RenderMap",0)
	Scrap.Set("R_SkipUpdateVis", 1)

def showmap():
	Scrap.Set("RenderSky",1)
	Scrap.Set("RenderMap",1)
	Scrap.Set("R_SkipUpdateVis", 0)
	
	
def membshow():
	Scrap.SetCallFunc("!PrintMemBlockData");Scrap.Execute()

def escrinchot(id=0):
	Scrap.ScreenShot("")
	
def sshot():
	
	SInput.Bind(0,"Racer","Scorer","QuickConsole.escrinchot")
	SInput.Bind(0,"Walk", "NextAction","QuickConsole.escrinchot")
	
	SInput.AssingEntry("Kb","SPACE",0,"Racer","Scorer")
	SInput.AssingEntry("Kb","SPACE",0,"Walk","NextAction")

######################################## TRAFICATOR ########################################
ACTRAK = None
ACID   = None
ACNAM  = None
def traff(aidi=0):
	global ACTRAK,ACNAM,ACID
	
	Scrap.UsrEntity(0).WRadius = Scrap.Get("NTrafficCapsule")

	ACTRAK = 0
	ACID   = aidi
	ACNAM  = "DM_Track"+`ACID`+"_"
	
	SInput.Bind(0,"Racer","TargetNext","QuickConsole.printhtraff")
	SInput.Bind(0,"Racer","NextWeapon","QuickConsole.nextraff")
	SInput.Bind(0,"Racer","PrevWeapon","QuickConsole.prevtraff")
	
	SInput.Bind(0,"Racer","Laser",     "QuickConsole.ligttraff")
	SInput.Bind(0,"Racer","Vulcan",    "QuickConsole.mediumtraff")
	SInput.Bind(0,"Racer","Devastator","QuickConsole.heavytraff")
	SInput.Bind(0,"Racer","Swarm",     "QuickConsole.smallesttraff")
	
	
	Scrap.Set("Debug",1)
	
	Scrap.ConsoleError("================ [ "+ACNAM+" ] ================\n")

def printhtraff(id = 0):
	global ACTRAK,ACNAM,ACID
	me = Scrap.UsrEntity(0)
	Pos = SVec.Prod(me.Pos,1.0)
	rad = me.WRadius*1.0
	Scrap.ConsoleError('Dummy boxsize:['+`rad`+','+`rad`+','+`rad`+'] pos:['+`Pos[0]`+","+`Pos[2]`+","+`Pos[1]`+'] isSelected:on name:"'+ACNAM+`ACTRAK`+'"\n')
	SScorer.SetLabelText(ACNAM+`ACTRAK`,Scrap.GetTime()+5)
	ACTRAK = ACTRAK+1
	
def nextraff(id=0):
	global ACTRAK,ACNAM,ACID
	
	ACID   = ACID+1
	ACNAM  = "DM_Track"+`ACID`+"_"
	ACTRAK = 0
	SScorer.SetLabelText(ACNAM+"#",Scrap.GetTime()+5)
	
	Scrap.ConsoleError("================ [ "+ACNAM+" ] ================\n")
	
def prevtraff(id=0):
	global ACTRAK,ACNAM,ACID
	
	ACID   = ACID-1
	ACNAM  = "DM_Track"+`ACID`+"_"
	ACTRAK = 0
	SScorer.SetLabelText(ACNAM+"#",Scrap.GetTime()+5)
	
	Scrap.ConsoleError("================ [ "+ACNAM+" ] ================\n")

def smallesttraff(id=0): 
	global ACTRAK,ACNAM,ACID
	
	SScorer.SetLabelText("Smallest traffic",Scrap.GetTime()+5)
	Scrap.UsrEntity(0).WRadius = Scrap.Get("STrafficCapsule")
	
def ligttraff(id=0):
	global ACTRAK,ACNAM,ACID
	
	SScorer.SetLabelText("Light traffic",Scrap.GetTime()+5)
	Scrap.UsrEntity(0).WRadius = Scrap.Get("NTrafficCapsule")

def mediumtraff(id=0):
	global ACTRAK,ACNAM,ACID
	
	SScorer.SetLabelText("Medium traffic",Scrap.GetTime()+5)
	Scrap.UsrEntity(0).WRadius = Scrap.Get("MTrafficCapsule")

def heavytraff(id=0):
	global ACTRAK,ACNAM,ACID
	
	SScorer.SetLabelText("Heavy traffic",Scrap.GetTime()+5)
	Scrap.UsrEntity(0).WRadius = Scrap.Get("HTrafficCapsule")

############################################################################################
def shv():
	SScorer.SetLabelText(`int(Scrap.UsrEntity(0).Speed/28.0)`+" Km/h",Scrap.GetTime()+1)
	Scrap.AddScheduledFunc(Scrap.GetTime()+0.1,shv,())

def milio(SndName):
	import Milios	
	Milios.Init(SndName)
	free()
	
def race():
	import RacerTool
	RacerTool.Init()

def coin():
	import CoinTool
	CoinTool.Init()
	
	
#/dropitem "models/misc/Amplifier/Amplifier.m3d"
def dropitem(modelname):

	c = Scrap.GetEntity(Scrap.GetEntity(Scrap.GetCam(0)).Target)
	OffsetPos = 0,0,-1
	OffsetPos = SVec.Rotate3D(OffsetPos,c.Ang)
	OffsetPosNorm = SVec.Norm(OffsetPos)
	OffsetPos = SVec.Prod(OffsetPosNorm,1000)
	pos = SVec.Add(c.Pos,OffsetPos)
			
	l = Scrap.CreateEntity("DroppedItem", pos[0], pos[1], pos[2], "Item")
	l.SetModel(modelname)
	l.Radius        = 500
	l.AppearsIfFree = 0
	l.AutoFace      = 0
	l.OnlyUser      = 0
	l.rTime         = 0.25

############################################################################################
########                     Dedicated Server managment                             ########
############################################################################################

# /svr      : restarts
# /svr "FZ" : restarts and change the map
def svr(mapname = None):
	import Init
	
	if Scrap.GetTime() < 15:
		Scrap.Print("Wait 15 seconds before the reinitialization.\n")
		return
	
	if Scrap.GetNetFlags()[0]: # client case
		if mapname:
			SNet.SendCtrlString("/svr '"+mapname+"'")
		else:
			SNet.SendCtrlString("/svr")
		return
			
	if mapname:
		mapname = "Levels/"+mapname
	else:
		mapname = Init.Path
	
	if Scrap.GetNetFlags()[1]:
		SNet.ServerChangeLevel(mapname)
	else:
		SNet.InitServer(mapname,10,0)

# /plys list the current players...
def plys():
	import Ffa,string
	Scrap.Print("|-------+---+------+-------------------\n")
	Scrap.Print("|  Id   |pts| Ping |  Descriptor \n")
	Scrap.Print("|-------+---+------+-------------------\n")
	for s in Ffa.Scorer.keys():
		Scrap.Print("|%6s |%3d|%6d| '%s'\n"%(s,Ffa.Scorer[s],SNet.GetPing(s),Scrap.GetEntity(s).Descriptor))
	Scrap.Print("|-------+---+------+-------------------\n\n")

# /kick "<player id>" disconnects a client from the server
def kick(name):
	if Scrap.GetNetFlags()[0]: # client case
		SNet.SendCtrlString("/kick '"+name+"'")
		return
	
	if SNet.Kick(name):
		Scrap.Print(name+" is going to be kicked...\n")
	else:
		Scrap.Print(name+" cannot be kicked\n")

def say(cad):
	import Chat
	
	if Scrap.GetNetFlags()[0]: # client case
		SNet.SendCtrlString("/say '"+cad+"'")
		return
		
	if Scrap.GetNetFlags()==(0,1,1):
		SNet.SendChatString(-1,"svr\tSERVER: "+cad)
	else:
		Chat.SendChatString(0,cad,1)
	