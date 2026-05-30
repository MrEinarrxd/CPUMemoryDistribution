# Guia rapida de configuracion PVM

## 1. Configurar SSH sin internet

Esto se hace desde el nodo master. No necesita conexion a internet, solo que las maquinas se vean en la red local.

Generar llave SSH en el master:

```bash
ssh-keygen -t rsa -b 4096
```

Presionar Enter en las preguntas si se quiere usar la configuracion por defecto.

Copiar la llave publica a cada slave:

```bash
ssh-copy-id master@192.168.100.98
ssh-copy-id master@192.168.100.97
```

Si `ssh-copy-id` no esta disponible, copiar manualmente la llave publica:

```bash
cat ~/.ssh/id_rsa.pub
```

En cada slave, pegar esa linea en `~/.ssh/authorized_keys`:

```bash
mkdir -p ~/.ssh
nano ~/.ssh/authorized_keys
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
```

Probar que el master entra sin pedir contrasena:

```bash
ssh master@192.168.100.98
ssh master@192.168.100.97
```

## 2. Entrar a la consola de PVM

Desde el nodo master:

```bash
pvm
```

## 3. Reiniciar PVM si estaba abierto antes

Dentro de la consola de PVM:

```text
halt
```

Luego volver a entrar:

```bash
pvm
```

## 4. Agregar los nodos esclavos

Dentro de la consola de PVM, escribir un comando por linea:

```text
add 192.168.100.98 192.168.100.97
conf
quit
```

`quit` sale de la consola, pero deja `pvmd` corriendo.

## 5. Configurar variables para el simulador

Desde la terminal normal, en la carpeta del proyecto:

```bash
cd ~/Documents/GitHub/CPUMemoryDistribution
export PVM_SLAVE_HOSTS=192.168.100.98,192.168.100.97
export PVM_SLAVE_EXEC=/home/master/Documents/GitHub/CPUMemoryDistribution/pvmSlave
```

Opcionalmente se puede cambiar la cantidad de ciclos locales que se ejecutan antes del analisis distribuido:

```bash
export PVM_WARMUP_CYCLES=1000
```

## Recompilar ejecutables

Si se cambia el codigo, recompilar el master y el slave. El ejecutable `pvmSlave` debe existir tambien en ambos nodos esclavos.

```bash
gcc -std=c11 -Wall -Wextra -I./src src/domain/distributed/pvmSlave.c -o pvmSlave -lpvm3
gcc -std=c11 -Wall -Wextra -DpvmModeEnabled=1 -I./src \
  $(find src -name '*.c' ! -name 'pvmSlave.c') \
  -o simPvm -lpvm3
```

## 6. Ejecutar el simulador

```bash
./simPvm
```

Cuando pregunte el modo:

```text
Opcion: 2
```

## Verificar que los esclavos existen

```bash
ssh 192.168.100.98 'ls -l ~/Documents/GitHub/CPUMemoryDistribution/pvmSlave'
ssh 192.168.100.97 'ls -l ~/Documents/GitHub/CPUMemoryDistribution/pvmSlave'
```

Si no tienen permiso de ejecucion:

```bash
ssh 192.168.100.98 'chmod +x ~/Documents/GitHub/CPUMemoryDistribution/pvmSlave'
ssh 192.168.100.97 'chmod +x ~/Documents/GitHub/CPUMemoryDistribution/pvmSlave'
```

## Errores comunes

`pvm_mytid(): Can't contact local daemon`

Significa que PVM no esta corriendo. Ejecutar:

```bash
pvm
```

Luego dentro de PVM:

```text
add 192.168.100.98 192.168.100.97
quit
```

`pvm_spawn: Not Found`

Significa que PVM no encontro el ejecutable esclavo. Ejecutar:

```bash
export PVM_SLAVE_EXEC=/home/master/Documents/GitHub/CPUMemoryDistribution/pvmSlave
```

Tambien verificar que `pvmSlave` exista en ambos nodos esclavos.
