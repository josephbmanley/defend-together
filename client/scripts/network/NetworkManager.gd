extends Node

var server_host : String = "127.0.0.1"

const SERVER_PORT : int = 7777

signal disconnection
signal connection_established
signal error_occured
signal logged_in

signal world_data_recieved
signal chat_message_recieved

var username : String = ""

var client : GDNetHost = null
var peer : GDNetPeer = null
var server_address : GDNetAddress = null
var connected : bool = false

var packetQueue = []
var error_info = ""
var world_data : String = ""

var last_move_time = null

var connection_timer : Timer

func _init():
	server_address = GDNetAddress.new()
	
	client = GDNetHost.new()
	client.set_max_channels(2)
	client.set_max_peers(1)
	client.set_event_wait(250)
	client.bind()

func connect_to_server():
	if peer:
		peer.disconnect_now()
	server_address.set_host(server_host)
	server_address.set_port(SERVER_PORT)
	peer = client.host_connect(server_address)
	if not connection_timer:
		connection_timer = Timer.new()
		add_child(connection_timer)
		connection_timer.connect("timeout", self, "timeout_check")
	connection_timer.start(1)
		
func timeout_check():
	if username == "":
		display_error("Connection failed!")
	connection_timer.stop()
	
func request_world_map():
	var request_packet : PoolByteArray = "2|".to_ascii()
	send_packet(request_packet)
	
func connect_as_user(username : String):
	connect_to_server()
	var username_packet : PoolByteArray = ("1|" + username).to_ascii()
	
	send_packet(username_packet)

func display_error(error = "Unknown error occured!"):
	error_info = error
	print("Error: " + error)
	emit_signal("error_occured")
	
func disconnect_from_server():
	peer.disconnect_later()
	
func process_events():
	if(client.is_event_available()):
		var event : GDNetEvent = client.get_event()
		
		var event_type = event.get_event_type()
		
		if(event_type == GDNetEvent.RECEIVE):
			if event.get_channel_id() == 0:
				var ascii_data : String = str(event.get_packet().get_string_from_ascii())
				if len(ascii_data) > 0:
					if ascii_data[0] == '1':
						if ascii_data.substr(2,2) == "OK":
							username = ascii_data.substr(4)
							print("Logged in as: " + username)
							emit_signal("logged_in")
						else:
							display_error("Username not accepted! Reason: " + ascii_data.substr(2))
					elif ascii_data[0] == '2':
						world_data = ascii_data.substr(2)
						emit_signal("world_data_recieved")
			elif event.get_channel_id() == 1:
				var chat_message : String = str(event.get_packet().get_string_from_ascii())
				emit_signal("chat_message_recieved", chat_message)
		elif(event_type == GDNetEvent.CONNECT):
			print("Connected to server with hostname: " + server_address.get_host() + ":" + str(server_address.get_port()))
			connected = true
			emit_signal("connection_established")
		elif(event_type == GDNetEvent.DISCONNECT):
			print("Disconnected from server")
			connected = false
			emit_signal("disconnection")
			

func send_chat_message(msg : String):
	var pckt : PoolByteArray = (msg + '\n').to_ascii()
	send_packet(pckt, 1)

func send_packet(packet : PoolByteArray, channel = 0, pck_type = GDNetMessage.RELIABLE):
	packetQueue.append({'channel':channel, 'packet' : packet, 'type' : pck_type})
			
func move_player(x,y):
	if last_move_time == null || OS.get_ticks_msec() - last_move_time > 50:

		var pckt : PoolByteArray = ("3|" + str(x) + "," + str(y)).to_ascii()
	
		send_packet(pckt, 0, GDNetMessage.SEQUENCED)
		last_move_time = OS.get_ticks_msec()
	
func _process(delta):
	process_events()
	
	if len(packetQueue) > 0 and connected:
		peer.send_packet(packetQueue[0]['packet'], packetQueue[0]['channel'], packetQueue[0]['type'])
		packetQueue.remove(0)
		
	
	if(Input.is_action_just_pressed("ui_cancel")):
		print("PACKET")
		peer.send_packet("TEST".to_ascii(), 0, GDNetMessage.RELIABLE)

