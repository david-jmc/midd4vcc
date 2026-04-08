# Midd4VCC (Middleware for Vehicular Cloud Computing)

Midd4VCC is a middleware designed to support the creation and management of **Vehicular Clouds (VC)** and **Intelligent Transportation Systems (ITS)**. It provides seamless and scalable communication and coordination among distributed vehicular entities (e.g., vehicles, roadside units, and transportation systems) by mediating communication and enabling the distribution and coordination of job offloading in cooperative vehicular settings. It integrates topic-based messaging, telemetry exchange, event-driven processing, and dynamic service interaction.

The middleware consists of two main components:
* **Midd4VCClient:** Executes on the client side (vehicles, application clients, sensors, and RSUs). Application clients (e.g., Pedestrian Nodes) publish jobs without needing to specify the intended recipients.
* **Midd4VCServer:** Responsible for mediating communication, monitoring registers, and distributing task execution across the VC entities via MQTT.

---

## 🛠️ Prerequisites

Before compiling, you must install the development dependencies and the MQTT communication broker.

### 1. Libraries and Tools
On Ubuntu/Debian, run:
```bash
sudo apt update
sudo apt install -y build-essential cmake libpaho-mqtt-dev mosquitto mosquitto-clients```

### 2. MQTT Broker
The MQTT Broker, e.g., mosquitto service must be active to enable communication between the nodes and the server:
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto```

## Compilation
The project uses CMake to manage dependencies. Navigate to the project root folder:
```bash
mkdir build && cd build
cmake ..
make```

This will generate the Midd4VCServer binary executable, run the follows command:
```bash
./Midd4VCServer```