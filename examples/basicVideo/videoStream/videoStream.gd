extends TextureRect
signal finished

export(String,FILE) var videoPath = "res://videos/big_buck_bunny.mp4"
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
var curTime = -1
var curVidTime = -1
var curAudioTime = -1
var curVidPath = ""
var hasVideo
var hasAudio
var initialzed = false
var pauseTime = 0
var gotLastFrame = false
var threadOn = false
var width
var height

var dataToProc = null
var imageToProc =null
onready var audio : AudioStreamPlayer = $audio
export var autoPlay = true
var frameBufferSize = 300
export var audioBufferLength = 0.5
export var dontProcessAudio = false
export var looped = false
export var proccesThread = false

var theTexture 
var tBuffer = null
var audioThread = Thread.new()
var videoThread = Thread.new()
var playbackSpeed = 1
var thread : Thread

func _ready():
	
	if proccesThread:
		thread = Thread.new()
	
	if videoPath != "":
		isPlaying = autoPlay
		loadVideo(videoPath)
	
	
	


func loadVideo(path):
	close()
	
	var globalPath  = ProjectSettings.globalize_path(path)

	var ret = $Node.loadFile(globalPath)

	var err = ret["error"]
	
	if err < 0:
		print("Error opening video file:" + path)
		return
	
	var dim  =$Node.getDimensions()
	width = dim.x
	height = dim.y
	hasAudio = ret["hasAudio"]
	hasVideo = ret["hasVideo"]
	
	
	image = Image.new()
	image.create(dim.x,dim.y,false,Image.FORMAT_RGBA8)
	image.lock()
	texture.image = image
	image.unlock()
	
	duration = $Node.getDuration()
	
	var audioInfo = $Node.getAudioInfo()
	var channels = audioInfo[1]
	var smapleRate = audioInfo[2]
	var audioStream = AudioStreamGenerator.new()
	
	audioStream.mix_rate = smapleRate
	audioStream.buffer_length = audioBufferLength
	audio.stream = audioStream
	audioGen = audio.get_stream_playback()
	curVidPath = path
	initialzed = true
	isPlaying = autoPlay
	pauseTime = 0
	
	if proccesThread:
		threadOn = true
		videoThread.start(self,"thread_process")

func _process(delta):
	
	if proccesThread:
		if imageToProc != null:
			
			var t = ImageTexture.new()
			t.create_from_image(imageToProc)
			if t.get_data() != null:
				texture = t
				imageToProc = null

		return
	
	if !initialzed: 
		return
	
	if isOver and looped:
		seek(0)
		if !looped:
			isPlaying = true
		
		

	
	if !isPlaying:
		if audio.playing:
			audio.stream_paused = true
		return
	else:
		audio.stream_paused =false

	$Node.process()
	
	if startTime ==-1:
		startTime = OS.get_system_time_msecs() 

	if !isOver and hasVideo:
		renderUpdate()
		
	if !dontProcessAudio and hasAudio:
		processAudio()


	
func thread_process():
	while(true):
		if threadOn == false:
			return
			
		if !initialzed: 
			OS.delay_msec(150)
			continue
		
		if isOver and looped:
			seek(0)

		
		if !isPlaying:
			OS.delay_msec(150)
			continue

		var res = $Node.process()


		if startTime ==-1:
			startTime = OS.get_system_time_msecs() 

		if !isOver and hasVideo:
			renderUpdate()
		
		if !dontProcessAudio and hasAudio:
			processAudio()


func renderUpdate():
	if !initialzed:
		return 
	curVidTime =  $Node.getCurVideoTime()

	curTime = getTime() - pauseTime
	var itt = 0
	
	if (curTime) > duration and duration > 0 and $Node.getImageBufferSize() == 0:
		curVidTime = duration
		isOver = true
		return
		
	if (curTime  >= curVidTime):
		

		if itt == 4:
			playbackSpeed -= 0.03

		
		if curTime >= curVidTime:
			
			if $Node.getImageBufferSize() > 0:
				
				var data = $Node.popRawBuffer()
				
				
				var image = Image.new()
				image.create_from_data(width,height,false,Image.FORMAT_RGBA8,data)
				if !proccesThread:
					var t = ImageTexture.new()
					t.create_from_image(image)
					texture = t
				else:
					imageToProc = image

				
				if prevPoolId !=-1:
					$Node.clearPoolEntry(prevPoolId)
				
			else:
				videoProcess()#we need to fill up buffer
				curVidTime = $Node.getCurVideoTime() 
		
		curVidTime = $Node.getCurVideoTime()
		curTime = getTime()- pauseTime
		

		#if curVidTime < -2:
		#	breakpoint
		
		
		itt += 1


func processAudio():
	
	curAudioTime =  $Node.getCurAudioTime()
	curTime = getTime() - pauseTime
	

	if (curTime < curAudioTime-audioBufferLength+0.16) or dontProcessAudio:
		return

	var frameAvail = audioGen.get_frames_available()
	var genBufferSize = $Node.getAudioBufferSize() > 0
	
	if frameAvail == 0:
		return
		
	if ($Node.getAudioBufferSize() > 0 and frameAvail > $Node.getAudioBufferSize()):
		var out =$Node.popAudioBuffer()

		curAudioTime = $"Node".getCurAudioTime()
		audioGen.push_buffer(out)


	if !audio.playing:
		audio.play()


func seek(timeSec):
	isPlaying = false
	$Node.seek(timeSec)
	
	audio.stop()
	audioGen.clear_buffer()
	
	curVidTime = $Node.getCurVideoTime()
	startTime = OS.get_system_time_msecs() - timeSec*1000.0
	pauseTime = 0
	isOver = false
	isPlaying = true
	 
	
func close():
	if initialzed:
		startTime = -1
		latestTime = 0
		duration = -1
		prevPoolId = -1
		curVidTime = -1
		initialzed = false
		
		
		if proccesThread:
			threadOn = false
			videoThread.wait_to_finish()
		$Node.close()
		


func videoProcess():
	if $Node.process() == -1:
		pauseTime = 0
		emit_signal("finished")

func getTime():
	return ((OS.get_system_time_msecs() -startTime) / 1000.0)*playbackSpeed
	
