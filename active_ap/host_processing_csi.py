import socket
import numpy as np
import collections
import matplotlib.pyplot as plt
import time

UDP_IP = "192.168.4.2"
UDP_PORT = 8848

QUEUE_LEN = 50
CSI_LEN = 57 * 2
DISP_FRAME_RATE = 10 # 10 frames per second

node_mac_list = []
corlor_list = []

# lists to hold 'artists' from matplotlib
curve_rssi_list = []
scatter_rssi_list = []
text_rssi_list = []
curve_csi_list = []

def parse_data_line (line, data_len) :
    data = line.split(",") # separate by commas
    assert(data[-1] == "")
    data = [ int(x) for x in data[0:-1] ] # ignore the last one
    assert(len(data) == data_len)

    return data

def add_new_node (node_id):
    rssi_que_list.append( collections.deque(np.zeros(QUEUE_LEN)) )
    csi_points_list.append( np.ones(CSI_LEN) )
    # new rssi curve
    (curve_rssi,) = ax_rssi.plot(disp_time, rssi_que_list[node_id], animated=True)
    curve_rssi_list.append(curve_rssi)
    scatter_rssi = ax_rssi.scatter( disp_time[-1], rssi_que_list[node_id][-1], animated=True)
    scatter_rssi_list.append(scatter_rssi)
    text_rssi = ax_rssi.text( disp_time[-2], rssi_que_list[node_id][-1]+2, "{} dB".format(rssi_que_list[node_id][-1]), animated=True)
    text_rssi_list.append(text_rssi)
    # new csi curve
    (curve_csi,) = ax_csi.plot(csi_points_list[node_id], animated=True)
    curve_csi_list.append(curve_csi)
    

def parse_data_packet (data) :
    data_str = str(data, encoding="ascii")
    lines = data_str.splitlines()
    node_id = -1
    for l_count in range(len(lines)):
        line = lines[l_count]
        print(line)
        items = line.split(",")

        if items[0].find("mac =") >= 0:
            mac_addr = items[0][items[0].find("mac =") + 5:]
            # if a new mac addr
            if not mac_addr in node_mac_list:
                node_mac_list.append(mac_addr)
                node_id = len(node_mac_list) - 1
                add_new_node(node_id)
            else:
                node_id = node_mac_list.index(mac_addr)

        if items[0] == "rx_ctrl info":
            # the next line should be rx_ctrl info.
            tmp_pos = items[1].find("len = ")
            rx_ctrl_len = int(items[1][tmp_pos+6:]) 
            # parse rx ctrl data
            rx_ctrl_data = parse_data_line(lines[l_count + 1], rx_ctrl_len)

        if items[0] == "RAW" :
            # the next line should be raw csi data.
            tmp_pos = items[1].find("len = ")
            raw_csi_len = int(items[1][tmp_pos+6:])
            # parse csi raw data
            raw_csi_data = parse_data_line(lines[l_count + 1], raw_csi_len)
    # a newline to separate packets
    print()

    return ( rx_ctrl_data, raw_csi_data, node_id)

# scale csi data accoding to SNR
# change to numpy array as well
def cook_csi_data (rx_ctrl_info, raw_csi_data) :
    rssi = rx_ctrl_info[0]  # dbm
    noise_floor = rx_ctrl_info[11] # dbm. The document says unit is 0.25 dbm but it does not make sense.
    # do not know AGC

    # Each channel frequency response of sub-carrier is recorded by two bytes of signed characters. 
    # The first one is imaginary part and the second one is real part.
    raw_csi_data = [ (raw_csi_data[2*i] * 1j + raw_csi_data[2*i + 1]) for i in range(int(len(raw_csi_data) / 2)) ]
    raw_csi_array = np.array(raw_csi_data)    

    ## Note:this part of SNR computation may not be accurate.
    #       The reason is that ESP32 may not provide a accurate noise floor value.
    #       The underlying reason could tha AGC is not calculated explicitly 
    #       so ESP32 doc just consider noise * 0.25 dbm as a estimated value. (described in the official doc)
    #       But here I will jut use the noise value in rx_ctrl info times 1 dbm as the noise floor.
    # scale csi
    snr_db = rssi - noise_floor # dB
    snr_abs = 10**(snr_db / 10.0) # from db back to normal
    csi_sum = np.sum(np.abs(raw_csi_array)**2)
    num_subcarrier = len(raw_csi_array)
    scale = np.sqrt((snr_abs / csi_sum) * num_subcarrier)
    raw_csi_array = raw_csi_array * scale
    print("SNR = {} dB".format(snr_db))
    #

    # TODO: delete pilot subcarriers
    # Note:
    #   check https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html
    #   section 'Wi-Fi Channel State Information' 
    #   sub-carrier index : LLTF (-64~-1) + HT-LTF (0~63,-64~-1)
    # In the 40MHz HT transmission, two adjacent 20MHz channels are used. 
    # The channel is divided into 128 sub-carriers. 6 pilot signals are inserted in sub-carriers -53, -25, -11, 11, 25, 53. 
    # Signal is transmitted on sub-carriers -58 to -2 and 2 to 58.
    assert(len(raw_csi_array) == 64 * 3)
    cooked_csi_array = raw_csi_array[64:]
    # rearrange to -58 ~ -2 and 2 ~ 58.
    cooked_csi_array = np.concatenate((cooked_csi_array[-58:-1], cooked_csi_array[2:59]))
    assert(len(cooked_csi_array) == CSI_LEN)
    
    print("RSSI = {} dBm\n".format(rssi))
    return (snr_db, cooked_csi_array)

# prepare plot
def preapre_fig ():
    # animated=True tells matplotlib to only draw the artist when we
    # explicitly request it
    ax_rssi.set_ylim(10, 50)
    (curve_rssi,) = ax_rssi.plot(disp_time, rssi_que_list[0], animated=True)
    curve_rssi_list.append(curve_rssi)
    scatter_rssi = ax_rssi.scatter( disp_time[-1], rssi_que_list[0][-1], animated=True)
    scatter_rssi_list.append(scatter_rssi)
    text_rssi = ax_rssi.text( disp_time[-2], rssi_que_list[0][-1]+2, "{} dB".format(rssi_que_list[0][-1]), animated=True)
    text_rssi_list.append(text_rssi)
    ax_rssi.set_xlabel("Time (s)")
    ax_rssi.set_ylabel("SNR (dB)")


    (curve_csi,) = ax_csi.plot(csi_points_list[0], animated=True)
    curve_csi_list.append(curve_csi)
    ax_csi.set_ylim(10, 50)
    ax_csi.set_xlim(0, CSI_LEN)
    ax_csi.set_xlabel("subcarriers [-58, -2] and [2, 58] ")
    ax_csi.set_ylabel("CSI (dB)")

    # make sure the window is raised, but the script keeps going
    plt.show(block=False)
    plt.pause(0.1) # stop to render backgroud


if __name__ == "__main__":
    # a queue to hold SNR values
    rssi_que_list = [collections.deque(np.zeros(QUEUE_LEN))]
    csi_points_list = [np.ones(CSI_LEN)]

    # create plot figure
    fig = plt.figure(figsize=(12,6))
    ax_rssi = plt.subplot(121)
    ax_csi = plt.subplot(122)

    # prepare fig
    disp_time = np.array([ (x - QUEUE_LEN + 1)/ DISP_FRAME_RATE for x in range(QUEUE_LEN)])
    preapre_fig()

    # get clean backgroud
    bg_clean = fig.canvas.copy_from_bbox(fig.bbox)


    # create a recv socket for packets from ESP32 soft-ap
    sock = socket.socket(socket.AF_INET, # Internet
                        socket.SOCK_DGRAM) # UDP
    sock.bind((UDP_IP, UDP_PORT))

    # record start time
    t_start = time.time()
    frame_count = 0

    while True:
        # recv UDP packet
        data, addr = sock.recvfrom(2048) # buffer size is 2048 bytes

        # parse data packet to get lists of data
        (rx_ctrl_data, raw_csi_data, node_id) = parse_data_packet(data)
        # only process HT(802.11 n) and 40 MHz frames
        # sig-mod and channel bandwidth fields
        if rx_ctrl_data[2] != 1 or rx_ctrl_data[4] != 1 :
            continue
        # node_id not assigned, error
        assert(node_id >= 0)
        print("Got a HT 40MHz packet ...")

        # prepare csi data
        (rssi, csi_data) = cook_csi_data(rx_ctrl_data, raw_csi_data)

        # update RSSI
        print("node id = ", node_id)
        rssi_que_list[node_id].popleft()
        rssi_que_list[node_id].append( rssi )
        # update CSI
        csi_points_list[node_id] = 10 * np.log10(np.abs(csi_data)**2)

        # plot rssi and CSI
        # 1. restore background
        fig.canvas.restore_region(bg_clean)
        # 2. update artist class instances
        curve_rssi = curve_rssi_list[node_id]
        rssi_que = rssi_que_list[node_id]
        scatter_rssi = scatter_rssi_list[node_id]
        text_rssi = text_rssi_list[node_id]

        curve_rssi.set_ydata(rssi_que)
        scatter_rssi.set_offsets( (disp_time[-1] , rssi_que[-1]) )
        text_rssi.set_position( (disp_time[-1], rssi_que[-1]+2) )
        text_rssi.set_text("{} dB".format(rssi_que[-1]) )

        curve_csi_list[node_id].set_ydata(csi_points_list[node_id])
        # 3. draw
        for n in range(len(node_mac_list)):
            ax_rssi.draw_artist(curve_rssi_list[n])
            ax_rssi.draw_artist(scatter_rssi_list[n])
            ax_rssi.draw_artist(text_rssi_list[n])
            ax_csi.draw_artist(curve_csi_list[n])

        # copy the image to the GUI state, but screen might not be changed yet
        fig.canvas.blit(fig.bbox)
        # flush any pending GUI events, re-painting the screen if needed
        # fig.canvas.flush_events()
        
        # you can put a pause in if you want to slow things down
        plt.pause(0.00001)

        # matplotlib is very slow ... Could be the reason of MacOS
        # only 15 fps on my macbook
        frame_count += 1
        print('Mean Frame Rate: {fps:.3f}FPS '.format(fps= (frame_count / (time.time() - t_start)) ) )
    


