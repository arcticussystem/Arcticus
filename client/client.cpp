#include "client.h"

using namespace cv;
using namespace std;

#define MAX_LEN 65535
#define MAX_NR 10
#define JOY_SLEEP 0.05

int socket_desc;
struct sockaddr_in global_to_addr;





static volatile int keep_running = true;
void handler (int arg) {
    keep_running = false;
    struct ctrl_msg control_signal = {};
    int bytes = sendto(socket_desc, (struct ctrl_msg*)&control_signal, sizeof(control_signal), 0, (struct sockaddr*)&global_to_addr, sizeof(global_to_addr));
    if (bytes == -1) {
        perror("sendto");
        exit(1);
    }
    exit(1);
}
int lin_map (float value, float x_0, float y_0, float x_1, float y_1) {
    int y;
    float k = (y_1 - y_0)/(x_1 - x_0);
    float m = y_0 - k*x_0;
    y = k*value + m;
    if (y > 1024) {
        y = 1024;
    }
    return y;
}
void* send_ctrl_msg (void* arg) {
    struct sockaddr_in* to_addr = (struct sockaddr_in*)arg;
    int bytes;
    struct ctrl_msg control_signal = {};
    int back[] = { 0, 1};
    int forward[] = { 1, 0};
    int scaling = 1;

    /* window */
    sf::RenderWindow window(sf::VideoMode(800, 600, 32), "Joystick Use", sf::Style::Default);
    sf::Event e;

    sf::Joystick::Identification id = sf::Joystick::getIdentification(0);
    std::cout << "\nVendor ID: " << id.vendorId << "\nProduct ID: " << id.productId << std::endl;
    sf::String controller("Joystick Use: " + id.name);
    window.setTitle(controller);

    window.setVisible(false);
    /* query joystick for settings if it's plugged in */
    if (sf::Joystick::isConnected(0)) {
        /* check how many buttons joystick number 0 has */
        unsigned int button_count = sf::Joystick::getButtonCount(0);

        /* check if joystick number 0 has a Z axis */
        bool haz_z = sf::Joystick::hasAxis(0, sf::Joystick::Z);

        std::cout << "Button count: " << button_count << std::endl;
        std::cout << "Has a z-axis: " << haz_z << std::endl;

    }

    /* for movement */
    sf::Vector2f speed = sf::Vector2f(0.f,0.f);

    while (window.pollEvent(e) || keep_running) {
        std::cout << "X axis: " << speed.x << std::endl;
        std::cout << "Y axis: " << speed.y << std::endl;

        if (speed.y > 0){ /* drive forward */
            control_signal.switch_signal_0 = forward[0];
            control_signal.switch_signal_1 = forward[1];
            control_signal.switch_signal_2 = forward[0];
            control_signal.switch_signal_3 = forward[1];
        } else { /* drive backwards */
            control_signal.switch_signal_0 = back[0];
            control_signal.switch_signal_1 = back[1];
            control_signal.switch_signal_2 = back[0];
            control_signal.switch_signal_3 = back[1];
           // scaling = 0.1;
        }
        int abs_vel = sqrt((speed.x*speed.x) + (speed.y*speed.y));
        int abs_mapped = lin_map(abs_vel, 0, 200, 100, 1024);
        if (keep_running == true){
            if (speed.x > 0) {
                control_signal.pwm_motor1 = ((100 - speed.x)/100)*abs_mapped;
                control_signal.pwm_motor2 = abs_mapped;
            } else {
                control_signal.pwm_motor2 = ((100 + speed.x)/100)*abs_mapped;
                control_signal.pwm_motor1 = abs_mapped;
            }
        } else {
            control_signal.pwm_motor1 = 0;
            control_signal.pwm_motor1 = 0;
            bytes = sendto(socket_desc, (struct ctrl_msg*)&control_signal, sizeof(control_signal), 0, (struct sockaddr*)to_addr, sizeof(*to_addr));
            if (bytes == -1) {
                perror("sendto");
                exit(1);
            }
            exit(1);
        }
        std::cout << "pwm1: " << control_signal.pwm_motor1 << std::endl;
        std::cout << "pwm2: " << control_signal.pwm_motor2 << std::endl;

        speed = sf::Vector2f(sf::Joystick::getAxisPosition(0, sf::Joystick::X), sf::Joystick::getAxisPosition(0, sf::Joystick::Y));
        bytes = sendto(socket_desc, (struct ctrl_msg*)&control_signal, sizeof(control_signal), 0, (struct sockaddr*)to_addr, sizeof(*to_addr));
        if (bytes == -1) {
            perror("sendto");
            exit(1);
        }
        if (cv::waitKey(25) >= 0) {
            control_signal.pwm_motor1 = 0;
            control_signal.pwm_motor2 = 0;
        }
        sleep(JOY_SLEEP);
    }
    return NULL;
}
void* receive_video (void* arg) {
    struct sockaddr_in* from_addr = (struct sockaddr_in*)arg;
    std::string encoded;

    while (keep_running) {
        socklen_t len = sizeof(from_addr);

        char str[MAX_LEN];
        char str_nr[MAX_NR];
        int index_stop;

        int bytes = recvfrom(socket_desc, str, MAX_LEN, 0, (struct sockaddr*)&from_addr, &len);
        if (bytes == -1) {
            perror("recvfrom");
            exit(1);
        }
        for (int i = 0; i < MAX_NR; i++) {
            if (str[i] == '/') {
                index_stop = i;
                break;
            }
        }
        strncpy(str_nr, str, index_stop);
        str_nr[index_stop] = '\0';
        /* Convert to std::string and remove the number in front */
        encoded = "";
        for (int i = index_stop; i < atoi(str_nr); i++) {
            encoded = encoded + str[i];
        }
        string dec_jpg = base64_decode(encoded); /* Decode the data to base 64 */
        std::vector<uchar> data(dec_jpg.begin(), dec_jpg.end()); /* Cast the data to JPG from base 64 */
        cv::Mat img = cv::imdecode(cv::Mat(data), 1); /* Decode the JPG data to class Mat */



        cv::imshow("Video feed", img);
    }
    return NULL;
}
int main(int argc, char** argv) {
    signal(SIGINT, handler); /* handles ctrl+C signal */
    int bytes;
    int port_nr;
    struct sockaddr_in to_addr;
    struct sockaddr_in my_addr;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "usage: %s destination port\n", argv[0]);
        exit(1);
    }

    /* extract destination IP address */
    struct hostent* host = gethostbyname(argv[1]);

    if (host == NULL) {
        fprintf(stderr, "unknown host %s\n", argv[1]);
        exit(1);
    }

    in_addr_t ip_address = *((in_addr_t*)(host->h_addr));

    /* extract destination port number */
    if (sscanf(argv[2], "%d", &port_nr) != 1) {
        fprintf(stderr, "invalid port %s\n", argv[2]);
        exit(1);
    }
    /* create UDP socket */
    socket_desc = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_desc == -1) {
        perror("socket");
        exit(1);
    }
    /* bound to any local address on the specified port */
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port_nr);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bytes = bind(socket_desc, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (bytes == -1) {
        perror("bind");
        exit(1);
    }
    /* allowing broadcast (optional) */
    int on = 1;
    bytes = setsockopt(socket_desc, SOL_SOCKET, SO_BROADCAST, &on, sizeof(int));
    if (bytes == -1) {
        perror("setsockopt");
        exit(1);
    }

    /* send message to the specified destination/port */
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(port_nr);
    to_addr.sin_addr.s_addr = ip_address;
    global_to_addr = to_addr;

    pthread_t receive_thread, send_thread;
    pthread_create(&send_thread, NULL, send_ctrl_msg, &to_addr);
    pthread_create(&receive_thread, NULL, receive_video, &to_addr);
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(socket_desc);
    return 0;
}