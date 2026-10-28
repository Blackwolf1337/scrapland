# The GOG sign-in and the videos without blocking waits (WinApp.c, App.c, ScrapEngMain.c, Video.c; execfile'd by
# sdcall.py, Python 1.5.2). The video functions now return VIDEO_PENDING while a video plays on from the message
# loop. In the test prefix the stream of Video/MSE.vds does not open (no XviD decoder: OpenFile fails in both runs),
# so these calls end at once with the result of the original: the paths found, not found, the "a|b" lists, the start
# video and Scrap.VideoPlayer. Not called: the stream itself (no video plays here), the init and the level load that
# wait for a video, the GOG sign-in wait (the start of the test passes it).
import Scrap
NWTVIDEO = 'Video/MSE.avi'
check('nwt GOG signed in', lambda: SDTest.Peek(0x44c108, 'b'))

check('nwt Video_PlayFile mse', lambda: SDTest.Call(0x281660, 'isz', NWTVIDEO, None)[0])
check('nwt Video_PlayFile missing', lambda: SDTest.Call(0x281660, 'isz', 'Video/SDNoVideo.avi', None)[0])
check('nwt Video_PlayFile list', lambda: SDTest.Call(0x281660, 'isz', NWTVIDEO + '|' + NWTVIDEO, None)[0])
check('nwt Video_PlayFile list second missing',
	lambda: SDTest.Call(0x281660, 'isz', NWTVIDEO + '|  Video/SDNoVideo.avi', None)[0])
check('nwt Video_PlayFile list first missing',
	lambda: SDTest.Call(0x281660, 'isz', 'Video/SDNoVideo.avi| ' + NWTVIDEO, None)[0])
check('nwt Video_PlayPath vds', lambda: SDTest.Call(0x281740, 'iszi', 'Video/MSE.vds', None, 0)[0])
check('nwt Video_PlayPath dir', lambda: SDTest.Call(0x281740, 'iszi', 'MSE.avi', 'Video/', 1)[0])

# the start video (g_StartVideo 0x4a8708, its flags 0x4a8704: bit 0 keeps the name after it played)
nwtname = SDTest.Peek(0x4a8708, 'p')
nwtflags = SDTest.Peek(0x4a8704, 'i')
def nwtstart(name, flags, play):
	SDTest.Call(0x134040, 'tviz', 0x8a8708, name)
	SDTest.Poke(0x4a8704, flags)
	return (SDTest.Call(0x216d10, 'bi', play)[0], SDTest.Peek(0x4a8708, 'p'))
check('nwt Video_PlayStart', lambda: nwtstart(NWTVIDEO, 0, 1))
check('nwt Video_PlayStart keep', lambda: nwtstart(NWTVIDEO, 1, 1))
check('nwt Video_PlayStart list', lambda: nwtstart(NWTVIDEO + '|' + NWTVIDEO, 0, 1))
check('nwt Video_PlayStart missing', lambda: nwtstart('Video/SDNoVideo.avi', 0, 1))
check('nwt Video_PlayStart no play', lambda: nwtstart(NWTVIDEO, 0, 0))
check('nwt Video_PlayStart empty', lambda: nwtstart('', 0, 1))
SDTest.Call(0x134040, 'tviz', 0x8a8708, nwtname)
SDTest.Poke(0x4a8704, nwtflags)

# Scrap.VideoPlayer through Video_Play (the video plays on from the message loop; here it ends at once)
check('nwt Video_Play', lambda: SDTest.Call(0x218010, 'bzsf', NWTVIDEO, 'play', 0.0)[0])
check('nwt Video_Play missing', lambda: SDTest.Call(0x218010, 'bzsf', 'Video/SDNoVideo.avi', 'play', 0.0)[0])
check('nwt Scrap.VideoPlayer', lambda: Scrap.VideoPlayer(NWTVIDEO))
# the callback of Scrap.VideoPlayer is an addition of the C (the original takes two arguments): the original run
# prints what the C must give, a call at once with 0 for a missing file and 1 for one that does not open here
nwtDone = []
def nwtplayer(name):
	del nwtDone[:]
	Scrap.VideoPlayer(name, 'play', lambda r: nwtDone.append(r))
	return nwtDone[:]
if SDTest.Peek(0x1985e0, 'b') == 0xe9:
	check('nwt Scrap.VideoPlayer done', lambda: (nwtplayer('Video/SDNoVideo.avi'), nwtplayer(NWTVIDEO)))
else:
	check('nwt Scrap.VideoPlayer done', lambda: ([0], [1]))
# a later call after all of these: none of them left a video playing (Video_PlayFile would stop at its CHECK)
check('nwt Video_PlayFile after', lambda: SDTest.Call(0x281660, 'isz', NWTVIDEO, None)[0])
