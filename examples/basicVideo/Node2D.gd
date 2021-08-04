extends TextureRect
signal finished

export(String,FILE) var videoPath = "‪"
var image
var startTime = -1
var latestTime = 0
var frameBuffer = []
var runningAudioBuffer : PoolVector2Array = []
var audioGen : AudioStreamGeneratorPlayback
var duration = -1
var isPlaying = true
var isOver = false
var prevPoolId = -1

var hasVideo
var hasAudio

onready var audio : AudioStreamPlayer = $audio
export var autoPlay = true
var frameBufferSize = 300
export var audioBufferLength = 0.25
export var dontProcessAudio = false
export var looped = false

var audioThread = Thread.new()
var videoThread = Thread.new()
var playbackSpeed = 1


func _ready():
	isPlaying = autoPlay
	loadVideo(videoPath)
	
	
var thread = Thread.new()

func loadVideo(path):
	
	var globalPath  = ProjectSettings.globalize_path(path)
	var ret = $Node.loadFile(globalPath)
	
	if ret["error"] < 0:
		print("Error opening video file")
		queue_free()
	
	var dim  =$Node.getDimensions()
	
	hasAudio = ret["hasAudio"]
	hasVideo = ret["hasVideo"]
	
	
	image = Image.new()
	image.create(dim.x,dim.y,false,Image.FORMAT_RGBA8)
	texture.image = image
	
	duration = $Node.getDuration()
	
	var audioInfo = $Node.getAudioInfo()
	var channels = audioInfo[1]
	var smapleRate = audioInfo[2]
	var audioStream = AudioStreamGenerator.new()
	
	audioStream.mix_rate = smapleRate
	audioStream.buffer_length = audioBufferLength
	audio.stream = audioStream
	audioGen = audio.get_stream_playback()
	
	

func _process(delta):

	if isOver and looped:
		seek(0)
	
	if !isPlaying:
		return
	
	
	$Node.process()
	
	if startTime ==-1:
		while($Node.getImageBufferSize() < 5 and hasVideo):
			videoProcess()

		startTime = OS.get_system_time_msecs() 
	

	
	if !isOver and hasVideo:
		renderUpdate()


	if !dontProcessAudio and hasAudio:
		processAudio()
	


func setImageThread(iamge,imageTarget):
	imageTarget.image = image



func renderUpdate():
	var curVidTime =  $Node.getCurVideoTime()
	var curTime = getTime()
	var itt = 0

	
	if (curTime  >= curVidTime):
		
		if itt == 4:
			playbackSpeed -= 0.03

		
		if curTime >= curVidTime:
			if $Node.getImageBufferSize() > 0:
				var arr = $Node.popImageBuffer()
				
				texture.image = arr[0]
				if prevPoolId !=-1:
					$Node.clearPoolEntry(prevPoolId)
					pass
				
				prevPoolId = arr[1]
				
			else:
				videoProcess()#we need to fill up buffer
				curVidTime = $Node.getCurVideoTime()

		curVidTime = $Node.getCurVideoTime()
		curTime = getTime()
		
		
		itt += 1


func processAudio():
	
	var curAudioTime =  $Node.getCurAudioTime()
	var curTime = getTime()
	
	if curTime < curAudioTime or dontProcessAudio:
		return

	
	var frameAvail = audioGen.get_frames_available()
	if frameAvail == 0:
		return
		
	var out =$Node.popAudioBuffer()
	
	if out.size() == 0:
		return
	
	audioGen.push_buffer(out)
	
	if !audio.playing:
		audio.play()


func seek(timeSec):
	$Node.seek(timeSec)
	startTime = OS.get_system_time_msecs()
	audio.stop()
	audioGen.clear_buffer()
	
	isOver = false
	isPlaying = true
	
	

func videoProcess():
	if $Node.process() == -1:
		isOver = true
		emit_signal("finished")

func getTime():
	return ((OS.get_system_time_msecs() -startTime) / 1000.0)*playbackSpeed
