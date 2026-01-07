# Midd4VC-C: Middleware for Vehicular Cloud Computing

**Midd4VC** is a middleware designed for **Vehicular Cloud Computing (VCC)** and **Intelligent Transport Systems (ITS)**. It provides a robust framework to manage computational tasks within a vehicular environment, enabling vehicles to act as service providers or consumers in a dynamic cloud architecture.

This repository contains the C implementation of the middleware, optimized for performance and low-level system integration.

---

## 🚀 Getting Started

### Prerequisites
Make sure you have the following installed:
* `git`
* `cmake`
* `gcc` (or any standard C compiler)
* `make`

### Installation & Compilation

First, clone the repository and navigate to the project directory:

```bash
git clone [https://github.com/david-jmc/midd4vc-c.git](https://github.com/david-jmc/midd4vc-c.git)
cd midd4vc-c
```

Now, build the project using CMake:

```bash
mkdir build
cd build
cmake ..
make
```

### 💻 Usage

After a successful build, you will find the executable files in the build folder.

1. Start the Server

The server application acts as the central orchestrator for the vehicular cloud:

```bash
./server_app
```

2. Simulate Vehicles (Vehicular Cloud)

To simulate multiple vehicles joining the cloud at specific coordinates, you can run the following loop (example with 3 vehicles):

```bash
# Start 3 simulated vehicles in the background
for i in {1..3}; do ./vehicle_app "V$i" 41.00$i -8.00$i & done

# To stop all simulated vehicles
killall vehicle_app
```

3. Simulate Client Applications

To simulate clients requesting computational tasks from the vehicular cloud:

```bash
# Start 3 simulated clients
for i in {1..3}; do ./client_app "C$i" & done

# To stop all simulated clients
killall client_app
```

### 📩 Contact
This project is developed at the Center for Informatics (CIn) - UFPE, Recife, Brazil. If you have any questions, suggestions, or would like to collaborate, feel free to reach out:

David J. M. Cavalcanti

Email: djmc@cin.ufpe.br

LinkedIn: [linkedin.com/in/seu-perfil](https://www.linkedin.com/in/davidcavalcanti/)

### 📄 License

This project is part of ongoing research in Vehicular Clouds and ITS. Please check the LICENSE file for more details.