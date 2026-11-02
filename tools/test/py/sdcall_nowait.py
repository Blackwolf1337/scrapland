# The videos without blocking waits (WinApp.c, App.c, ScrapEngMain.c, Video.c; execfile'd by
# sdcall.py, Python 1.5.2). The video functions now return VIDEO_PENDING while a video plays on from the message
# loop. NWTVIDEO is a file that is there but no video, so it opens in neither run (the reference has no XviD decoder
# in the test prefix, the C plays the real videos through FFmpeg) and these calls end at once with the result of the
# original: the paths found, not found, the "a|b" lists, the start video and Scrap.VideoPlayer. Not called: a stream
# that plays, the init and the level load that wait for a video.
import Scrap
NWTVIDEO = 'Scrap.cfg'
check('nwt Video_PlayFile mse', lambda: SDTest.Call('Video_PlayFile', 'isz', NWTVIDEO, None)[0])
check('nwt Video_PlayFile missing', lambda: SDTest.Call('Video_PlayFile', 'isz', 'Video/SDNoVideo.avi', None)[0])
check('nwt Video_PlayFile list', lambda: SDTest.Call('Video_PlayFile', 'isz', NWTVIDEO + '|' + NWTVIDEO, None)[0])
check('nwt Video_PlayFile list second missing',
	lambda: SDTest.Call('Video_PlayFile', 'isz', NWTVIDEO + '|  Video/SDNoVideo.avi', None)[0])
check('nwt Video_PlayFile list first missing',
	lambda: SDTest.Call('Video_PlayFile', 'isz', 'Video/SDNoVideo.avi| ' + NWTVIDEO, None)[0])
check('nwt Video_PlayPath missing quiet', lambda: SDTest.Call('Video_PlayPath', 'iszi', 'SDNoVideo.avi', 'Video/', 0)[0])
check('nwt Video_PlayPath dir', lambda: SDTest.Call('Video_PlayPath', 'iszi', 'fmod.dll', 'Bin/', 1)[0])

# the start video (g_StartVideo 0x4a8708, its flags 0x4a8704: bit 0 keeps the name after it played)
nwtname = SDTest.Peek('g_StartVideo', 'p')
nwtflags = SDTest.Peek('g_StartVideoFlags', 'i')
def nwtstart(name, flags, play):
	SDTest.Call('cString_Set', 'tviz', symva('g_StartVideo'), name)
	SDTest.Poke('g_StartVideoFlags', flags)
	return (SDTest.Call('Video_PlayStart', 'bi', play)[0], SDTest.Peek('g_StartVideo', 'p'))
check('nwt Video_PlayStart', lambda: nwtstart(NWTVIDEO, 0, 1))
check('nwt Video_PlayStart keep', lambda: nwtstart(NWTVIDEO, 1, 1))
check('nwt Video_PlayStart list', lambda: nwtstart(NWTVIDEO + '|' + NWTVIDEO, 0, 1))
check('nwt Video_PlayStart missing', lambda: nwtstart('Video/SDNoVideo.avi', 0, 1))
check('nwt Video_PlayStart no play', lambda: nwtstart(NWTVIDEO, 0, 0))
check('nwt Video_PlayStart empty', lambda: nwtstart('', 0, 1))
SDTest.Call('cString_Set', 'tviz', symva('g_StartVideo'), nwtname)
SDTest.Poke('g_StartVideoFlags', nwtflags)

# Scrap.VideoPlayer through Video_Play (the video plays on from the message loop; here it ends at once)
check('nwt Video_Play', lambda: SDTest.Call('Video_Play', 'bzsf', NWTVIDEO, 'play', 0.0)[0])
check('nwt Video_Play missing', lambda: SDTest.Call('Video_Play', 'bzsf', 'Video/SDNoVideo.avi', 'play', 0.0)[0])
check('nwt Scrap.VideoPlayer', lambda: Scrap.VideoPlayer(NWTVIDEO))
# the callback of Scrap.VideoPlayer is an addition of the C (the original takes two arguments): the original run
# prints what the C must give, a call at once with 0 for a missing file and 1 for one that does not open here
nwtDone = []
def nwtplayer(name):
	del nwtDone[:]
	Scrap.VideoPlayer(name, 'play', lambda r: nwtDone.append(r))
	return nwtDone[:]
if SDTest.Peek('Scrap_VideoPlayer', 'b') == 0xe9:
	check('nwt Scrap.VideoPlayer done', lambda: (nwtplayer('Video/SDNoVideo.avi'), nwtplayer(NWTVIDEO)))
else:
	check('nwt Scrap.VideoPlayer done', lambda: ([0], [1]))
# a later call after all of these: none of them left a video playing (Video_PlayFile would stop at its CHECK)
check('nwt Video_PlayFile after', lambda: SDTest.Call('Video_PlayFile', 'isz', NWTVIDEO, None)[0])
