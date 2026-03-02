#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "Arduino_GigaDisplay.h"
#include "lvgl.h"

#include "arduino_secrets.h"  // Para credenciales Wi-Fi
#include "thingProperties.h"  // Para variables y configuración de Arduino Cloud

// Incluimos las librerías del sistema de acceso
#include <Adafruit_Fingerprint.h>
#include <RGBLed.h>
#include <Arduino_USBHostMbed5.h>
#include <DigitalOut.h>
#include <FATFileSystem.h>
#include <mbed.h>  // Para mbed::Ticker

// Configuración del hardware
Arduino_H7_Video Display(480, 800, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;
GigaDisplayBacklight backlight;

//----------------Variables pantalla -----------------------

lv_obj_t *bat_label, *wifi_label;
int level_bat = 80;

// Variables para modo de autenticación
#define AUTH_MODE_NONE 0
#define AUTH_MODE_CLAVE 1
int auth_mode = AUTH_MODE_NONE;

// Contraseñas
const char* default_password = "1234";

// Variables globales para elementos UI
lv_obj_t *main_screen;
lv_obj_t *keypad_screen;
lv_obj_t *main_container;
lv_obj_t *lbl_hora, *lbl_fecha;
lv_obj_t *dateTimeContainer;

lv_obj_t *btn_timbre, *btn_ingreso, *btn_salida, *btn_clave;

// Variables para teclado
lv_obj_t *ta_password;
lv_obj_t *keypad;

// Configuración del sensor PIR y control de brillo con Ticker
const byte PIR_PIN = 2;
const uint8_t BRILLO_BAJO = 10;
const uint8_t BRILLO_ALTO = 100;
const float DURACION_BRILLO_ALTO_S = 60.0f;

volatile bool flagPIRDetectoMovimiento = false;
bool pantallaEnBrilloAlto = false;

mbed::Ticker apagarBrilloTicker;
volatile bool flagTimeoutBrilloOcurrido = false;

lv_obj_t *confirm_popup = NULL;
lv_obj_t *btn_confirm_ok, *btn_confirm_cancel;

// Prototipos de funciones
void inicializar_display();
void crear_pantalla_principal();
void crear_pantalla_teclado();
void crear_elementos_fecha_hora();
void crear_botones_principales();
void crear_iconos_estado();

void actualizarFechaHoraLVGL();
void actualizarIconoBateriaLVGL(int nivel);
void actualizarColorIconoWifiLVGL(int status);

void cambiar_a_pantalla(lv_obj_t *screen);
void limpiar_campos_texto();
void mostrar_teclado_numerico(lv_obj_t *ta_input, int modo);
lv_obj_t *crear_boton_basico(lv_obj_t *parent, int width, int height, const char *texto, const lv_font_t *fuente, lv_event_cb_t callback);
void animar_pulsacion_boton(lv_obj_t *btn, uint32_t color_original_hex_no_usado);

void manejadorInterrupcionPIR();
void callbackTickerApagarBrillo();
void procesarLogicaPIR();
void procesarTimeoutBrillo();

// NUEVO: Prototipo para la función de pop-up genérico
void mostrarPopup(const char *mensaje, const char *simbolo_icono, uint32_t duracion_segundos);
// NUEVO: Prototipo para el callback del timer del pop-up genérico
static void eliminar_popup_timer_cb(lv_timer_t *timer);

static void on_timbre_click(lv_event_t *e);
static void on_ingreso_click(lv_event_t *e);
static void on_salida_click(lv_event_t *e);
static void on_clave_click(lv_event_t *e);
static void on_back_click(lv_event_t *e);
static void on_keypad_click(lv_event_t *e);
static bool verificar_password(const char *password, const char *expected_pwd);

//-----------------------Variables programa---------------------------------------

// --- Constantes del Sistema de Acceso ---
#define SENSOR_SERIAL Serial1
#define MP3_SERIAL Serial2
#define FINGER_DETECT_PIN 9

#define PIN_R 3
#define PIN_G 6
#define PIN_B 5

#define TIEMPO_LED_ENCENDIDO 2000
#define LED_FADE_TIME 200
#define LED_FLASH_TIME 100
#define TIMEOUT_OPERACION_HUELLA 15000  // Timeout para operaciones internas de huella
#define RETARDO_ENTRE_INTENTOS 3000
#define TIMEOUT_USB_CONNECTION 10000

#define VOLUMEN_INICIAL 30
#define VOLUMEN_MAX 30

#define AUDIO_INGRESO_EXITOSO 1
#define AUDIO_SALIDA_EXITOSO 2
#define AUDIO_INGRESO_FALLIDO 3
#define AUDIO_SALIDA_FALLIDO 4
#define AUDIO_HUELLA_NO 5
#define AUDIO_HUELLA_SI 6
#define AUDIO_USER_DELETE "21"
#define AUDIO_USER_ADD 22
#define AUDIO_CONFIRT_DATA 23
#define AUDIO_ALARMA_DOOR 24
#define AUDIO_TIMBRE 37
#define AUDIO_BIENVENIDA 39
#define AUDIO_COLOQUE_DEDO 45
#define AUDIO_RETIRE_DEDO 46
#define AUDIO_COLOQUE_DEDO2 47
#define AUDIO_EXITO 53
#define AUDIO_ERROR 54
#define AUDIO_ABRIENDO_PUERTA 58
#define AUDIO_CERRANDO_PUERTA 59
#define CLAVE_CORRECTA 61
#define CLAVE_INCORRECTA 62

#define MAX_REINTENTOS_REGISTRO 3
#define MAX_USUARIOS_EN_MEMORIA 50
#define NOMBRE_MAX_LENGTH 31
#define DOCUMENTO_MAX_LENGTH 13
#define TELEFONO_MAX_LENGTH 13
#define FECHA_NACIMIENTO_LENGTH 11

// --- Estructura de Datos de Usuario (del sistema de acceso) ---
struct DatosUsuario {
  char nombre[NOMBRE_MAX_LENGTH];
  char documento[DOCUMENTO_MAX_LENGTH];
  char telefono[TELEFONO_MAX_LENGTH];
  char fechaNacimiento[FECHA_NACIMIENTO_LENGTH];
  uint8_t idHuella;
  bool activo;
};

// --- Estados del Sistema y Menú (Fusionados y Expandidos) ---
enum SystemState {
    IDLE,
    MAIN_MENU_AWAITING_CHOICE,

    ADD_USER_AWAITING_NAME,
    ADD_USER_AWAITING_DOCUMENT,
    ADD_USER_AWAITING_PHONE,
    ADD_USER_AWAITING_BIRTHDATE,
    ADD_USER_INITIATE_FINGERPRINT,
    ADD_USER_PROCESSING_FINGERPRINT,
    ADD_USER_SAVING,

    DELETE_USER_AWAITING_IDENTIFIER,
    DELETE_USER_CONFIRMING,
    DELETE_USER_PROCESSING,

    VIEW_USERS_DISPLAYING,
    ADJUST_VOLUME_AWAITING_LEVEL,
    WAKEUP_PROCESSING_FINGERPRINT,
    WAKEUP_WAITING_FOR_FINGER_REMOVAL,
    ADD_USER_CONFIRMING_DATA,
    INGRESO_ESPERANDO_HUELLA,
    INGRESO_PROCESANDO_HUELLA,
    SALIDA_ESPERANDO_HUELLA,
    SALIDA_PROCESANDO_HUELLA,
   
    // Nuevos estados para acceso personalizado
    AWAITING_ACCESS_NAME,
    AWAITING_ACCESS_START_DATE,
    AWAITING_ACCESS_END_DATE,
    PROCESSING_CUSTOM_ACCESS
};

SystemState currentSystemState = IDLE;
unsigned long lastInteractionTimeCloud = 0;
const unsigned long CLOUD_SESSION_TIMEOUT_MS = 120000;  // 2 minutos

// --- Variables Globales del Sistema de Acceso ---
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&SENSOR_SERIAL);
RGBLed accesorios_led(PIN_R, PIN_G, PIN_B, RGBLed::COMMON_CATHODE);
USBHostMSD msd;
mbed::FATFileSystem usb("usb");
bool usbConnected = false;
volatile bool dedoDetectadoFlag = false;
bool procesandoHuellaActual = false;
uint8_t intentosFallidosAcceso = 0;
uint8_t currentVolume = VOLUMEN_INICIAL;
unsigned long tiempoUltimaAccionDispositivo = 0;
bool sistemaInicializadoCorrectamente = false;
// ===== CORRECCION ERROR COMPILACION INICIO =====
bool sensorHuellasInicializadoOK = false;  // Variable para rastrear el estado del sensor
// ===== CORRECCION ERROR COMPILACION FIN =====
DatosUsuario usuarios[MAX_USUARIOS_EN_MEMORIA];
int totalUsuariosAlmacenados = 0;

DatosUsuario tempUserHolder;
uint8_t tempIdHuellaHolder;
char tempDocumentoHolder[DOCUMENTO_MAX_LENGTH];

// --- Variables de Tiempo de Arduino Cloud ---
mbed::Ticker actualizadorHoraTicker;
volatile bool actualizarHoraFlag = false;
const char *nombresDias[] = { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado" };
const char *nombresMeses[] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Sept.", "Oct.", "Nov.", "Dic." };
char globalFormattedDate[60];
char globalFormattedTime[20];

// --- Mensajes para el Chat ---
const char *MSG_INITIATE_MENU_PROMPT = "Bienvenido al Asistente de Acceso.\nEscribe 'menu' para comenzar o 'ayuda' para mas opciones.";
const char *MSG_MAIN_MENU = "Menu Principal:\n1. Registrar Usuario\n2. Eliminar Usuario\n3. Ver Usuarios\n4. Ajustar Volumen\n\nEscribe el numero de la opcion o 'cancelar'.";
const char *MSG_INVALID_OPTION_MENU = "Opcion no valida. Elige un numero del menu o 'cancelar'.";
const char *MSG_SESSION_TIMEOUT_CLOUD = "Sesion de chat finalizada por inactividad.\nEscribe 'menu' para volver a empezar.";
const char *MSG_CANCELLED_BY_USER = "Operacion cancelada.\nEscribe 'menu' para volver a empezar.";
const char *MSG_NOT_CONNECTED_CLOUD = "No conectado a Arduino Cloud. Funcionalidad de chat limitada.";
const char *MSG_GENERAL_ERROR_CLOUD = "Ha ocurrido un error en el chat. Intenta de nuevo.\nEscribe 'menu' para empezar.";
const char *MSG_USB_NOT_CONNECTED = "Error: Almacenamiento USB no conectado. Esta operacion no puede continuar.";
const char *MSG_MAX_USERS_REACHED = "Error: Limite de usuarios en memoria alcanzado.";

// ===== PROTOTIPOS DE FUNCIONES =====
void inicializarDispositivosAcceso();
void actualizarMaquinaEstadosDispositivo();
void manejarTimeoutOperacionDispositivo();
void inicializarAudio();
uint8_t calcularChecksumAudio(const uint8_t *command, size_t length);
bool enviarComandoAudio(const uint8_t *command, size_t length, uint8_t maxRetries = 1);
void reproducirPistaAudio(uint16_t trackNumber, bool esperarFinalizacion = false);
void configurarVolumenAudio(uint8_t volume, bool guardarVolumenActual = true);
void inicializarLEDsAccesorios();
void sincronizarLEDsConSensor(uint8_t colorSensor, uint8_t modoSensor, uint8_t velocidadSensor = LED_FLASH_TIME);
void configurarColorLED_Accesorios(uint8_t colorSensor);
void apagarTodosLEDs();
bool inicializarSensorHuellasR503();
uint8_t identificarHuellaRegistrada(uint16_t timeout = TIMEOUT_OPERACION_HUELLA);
bool registrarNuevaHuellaParaUsuario(uint8_t idHuellaDestino);
void procesarHuellaDetectadaPorWakeUp();
bool esperarRetiroDedoDelSensor(unsigned long timeout = TIMEOUT_OPERACION_HUELLA);
bool inicializarConexionUSB();
bool verificarConexionUSB();
bool cargarUsuariosDesdeUSB();
bool guardarUsuariosEnUSB();
int buscarIndiceUsuarioPorHuella(uint8_t idHuellaBuscada);
int buscarIndiceUsuarioPorDocumento(const char *documentoBuscado);
uint8_t obtenerSiguienteIdHuellaDisponible();
void crearDirectorioSiNoExisteEnUSB(const char *dirPath);
void ISR_DedoDetectado();
void setActualizarHoraFlagCloud();
void actualizarYAlmacenarHoraFormateadaCloud();
void enviarMensajeChat(const String &mensaje, bool appendInstruction = false);
void resetSessionCloud(const String &previousMessage = "");
void mostrarMenuPrincipalCloud();
void procesarOpcionMenuPrincipalCloud(const String &opcion);
void iniciarRegistroUsuarioCloud();
void procesarNombreUsuarioCloud(const String &nombre);
void procesarDocumentoUsuarioCloud(const String &documento);
void procesarTelefonoUsuarioCloud(const String &telefono);
void ejecutarRegistroHuellaCloud();
void finalizarRegistroUsuarioCloud(bool exitoHuella);
void iniciarEliminacionUsuarioCloud();
void procesarIdentificadorParaEliminarCloud(const String &identificadorStr);
void confirmarYEliminarUsuarioCloud(const String &confirmacion);
void mostrarUsuariosCloud();
void iniciarAjusteVolumenCloud();
void procesarNuevoVolumenCloud(const String &volStr);
void sendNotification(String message);


// --- Variables para el nuevo enfoque para las banderas de ingreso y salida---
mbed::Ticker ingreso_timer;
mbed::Ticker salida_timer;
volatile bool bandera_ingreso_activo = false;
volatile bool bandera_salida_activo = false;

void desactivar_ingreso_flag() {
  bandera_ingreso_activo = false;
  //apagarTodosLEDs();  // Podria corregirse despues pero si es necesario un cambio por que permanecen prendidos
}

void desactivar_salida_flag() {
  bandera_salida_activo = false;
  //apagarTodosLEDs();  // Apagado directo
}

//#define AUTH_MODE_NONE 0
#define AUTH_MODE_DEFAULT 1
#define AUTH_MODE_DYNAMIC 2
#define AUTH_MODE_CUSTOM 3
// Variables para clave dinámica
volatile bool updatePasswordFlag = false;
unsigned long lastPasswordUpdate = 0;
const unsigned long PASSWORD_UPDATE_INTERVAL = 3600000; // 1 hora en ms
mbed::Ticker passwordUpdateTicker;

void setUpdatePasswordFlag() {
  updatePasswordFlag = true; // Solo cambia la bandera
}

void generateNewPassword();
bool verificar_password(const char *password, int auth_mode);

//----------CAMBIOS PARA ACCESO PERSONALIZADO-----------------------------
#define NOMBRE_MAX_LENGTH 31
#define MAX_ACCESOS_PERSONALIZADOS 50
#define AUTH_MODE_CUSTOM 3

struct AccesoPersonalizado {
    char nombre[NOMBRE_MAX_LENGTH];
    uint16_t clave;
    char fechaInicio[11];  // Formato DD/MM/YYYY
    char fechaFin[11];     // Formato DD/MM/YYYY
};

AccesoPersonalizado accesosPersonalizados[MAX_ACCESOS_PERSONALIZADOS];
AccesoPersonalizado tempAccess;
uint16_t totalAccesosPersonalizados = 0;



// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1500);

  //---------inicializar pantalla---------------------
  backlight.begin();
  backlight.set(BRILLO_BAJO);
  pantallaEnBrilloAlto = false;
  inicializar_display();

  main_screen = lv_obj_create(NULL);
  keypad_screen = lv_obj_create(NULL);

  crear_pantalla_principal();
  crear_pantalla_teclado();

  pinMode(PIR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), manejadorInterrupcionPIR, RISING);

  cambiar_a_pantalla(main_screen);

  actualizarIconoBateriaLVGL(level_bat);
  actualizarColorIconoWifiLVGL(1);

  //-------------Inicializar Sistema----------------------------
  Serial.println(F("Iniciando Sistema de Control de Acceso con Arduino Cloud..."));

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  strcpy(globalFormattedDate, "Sincronizando...");
  Serial.println(F("Intentando sincronizacion inicial de tiempo con Cloud..."));
  if (TimeService.sync()) {
    Serial.println(F("Sincronizacion inicial de tiempo EXITOSA."));
    actualizarYAlmacenarHoraFormateadaCloud();
    Serial.println(globalFormattedDate);
  } else {
    Serial.println(F("FALLO la sincronizacion inicial de tiempo. Se reintentara."));
  }
  actualizadorHoraTicker.attach(setActualizarHoraFlagCloud, 60.0f);

  inicializarDispositivosAcceso();

  // Cargar accesos personalizados desde USB
  if (!cargarAccesosPersonalizadosDesdeUSB()) {
    Serial.println(F("Error al cargar accesos personalizados desde USB"));
  }

  if (ArduinoCloud.connected()) {
    enviarMensajeChat(MSG_INITIATE_MENU_PROMPT);
    actualizarColorIconoWifiLVGL(1);
  } else {
    Serial.println(F("ADVERTENCIA: No conectado a Arduino Cloud al finalizar setup."));
  }
  currentSystemState = IDLE;
  lastInteractionTimeCloud = millis();
  sistemaInicializadoCorrectamente = true;

  bandera_ingreso_activo = false;
  bandera_salida_activo = false;

  // Generar primera clave
  generateNewPassword();
 
  // Configurar Ticker para actualización periódica
  passwordUpdateTicker.attach(setUpdatePasswordFlag, std::chrono::milliseconds(PASSWORD_UPDATE_INTERVAL));

  ArduinoCloud.addProperty(clave_cloud, Permission::ReadWrite).onUpdate([](){
    Serial.print("[CLOUD] clave_cloud actualizada: ");
    Serial.println(clave_cloud);
  });
}
// ===== LOOP =====
void loop() {
  ArduinoCloud.update();
  lv_timer_handler();
  procesarLogicaPIR();
  procesarTimeoutBrillo();
  checkPasswordUpdate();

  if (actualizarHoraFlag) {
    actualizarHoraFlag = false;
    if (ArduinoCloud.connected()) {
      actualizarYAlmacenarHoraFormateadaCloud();
    }
  }

  if (currentSystemState != IDLE && currentSystemState != WAKEUP_PROCESSING_FINGERPRINT && currentSystemState != WAKEUP_WAITING_FOR_FINGER_REMOVAL) {
    if (millis() - lastInteractionTimeCloud > CLOUD_SESSION_TIMEOUT_MS) {
      Serial.println(F("Timeout de sesion de CHAT alcanzado."));
      resetSessionCloud(MSG_SESSION_TIMEOUT_CLOUD);
    }
  }

  if (dedoDetectadoFlag) {
    if (currentSystemState == IDLE || currentSystemState == MAIN_MENU_AWAITING_CHOICE || currentSystemState == ADD_USER_INITIATE_FINGERPRINT) {
      procesandoHuellaActual = true;
      dedoDetectadoFlag = false;
      detachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN));
      Serial.println(F("\nLOOP: DEDO DETECTADO (WAKE-UP / REGISTRO)"));

      if (currentSystemState == ADD_USER_INITIATE_FINGERPRINT) {
        Serial.println(F("LOOP: Dedo detectado durante ADD_USER_INITIATE_FINGERPRINT. Procediendo a registro de huella."));
        ejecutarRegistroHuellaCloud();
      } else {
        currentSystemState = WAKEUP_PROCESSING_FINGERPRINT;
      }
    } else {
      dedoDetectadoFlag = false;
      Serial.println(F("LOOP: Dedo detectado (wake-up) pero ignorado, sistema ocupado con chat."));
    }
  }

  switch (currentSystemState) {
    case WAKEUP_PROCESSING_FINGERPRINT:
      procesarHuellaDetectadaPorWakeUp();
      currentSystemState = WAKEUP_WAITING_FOR_FINGER_REMOVAL;
      tiempoUltimaAccionDispositivo = millis();
      break;

    case WAKEUP_WAITING_FOR_FINGER_REMOVAL:
      if (esperarRetiroDedoDelSensor(TIMEOUT_OPERACION_HUELLA / 2)) {
        attachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN), ISR_DedoDetectado, FALLING);
        procesandoHuellaActual = false;
        Serial.println(F("LOOP: Dedo retirado (wake-up). Volviendo a IDLE."));
        resetSessionCloud();
      } else if (millis() - tiempoUltimaAccionDispositivo > TIMEOUT_OPERACION_HUELLA / 2 + 2000) {
        Serial.println(F("LOOP: Timeout esperando retiro de dedo (wake-up). Forzando reinicio de detección."));
        attachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN), ISR_DedoDetectado, FALLING);
        procesandoHuellaActual = false;
        resetSessionCloud();
      }
      break;

    case ADD_USER_PROCESSING_FINGERPRINT:
      break;

    default:
      break;
  }

  //manejarTimeoutOperacionDispositivo();
}



//------Funciones PIR y Pantalla-------
void inicializar_display() {
  Display.begin();
  TouchDetector.begin();
}
void manejadorInterrupcionPIR() {
  flagPIRDetectoMovimiento = true;
}
void callbackTickerApagarBrillo() {
  flagTimeoutBrilloOcurrido = true;
}
void procesarLogicaPIR() {
  if (flagPIRDetectoMovimiento) {
    flagPIRDetectoMovimiento = false;
    backlight.set(BRILLO_ALTO);
    pantallaEnBrilloAlto = true;
    flagTimeoutBrilloOcurrido = false;
    apagarBrilloTicker.detach();
    apagarBrilloTicker.attach(&callbackTickerApagarBrillo, DURACION_BRILLO_ALTO_S);
  }
}
void procesarTimeoutBrillo() {
  if (flagTimeoutBrilloOcurrido) {
    flagTimeoutBrilloOcurrido = false;
    if (pantallaEnBrilloAlto) {
      backlight.set(BRILLO_BAJO);
      pantallaEnBrilloAlto = false;
    }
  }
}
void apagarTodosLEDs() {
  // 1. Apagar LED del sensor de huella (comando directo)
  finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_OFF, 0);

  // 2. Apagar LED RGB accesorio
  accesorios_led.off();

  // 3. Pequeña pausa para asegurar el comando
  delay(10);
}
void generateNewPassword() {
  randomSeed(analogRead(0) + micros() + millis());
  clave_cloud = random(1000, 9999); // Asignación directa a CloudInt
 
  lastPasswordUpdate = millis();
 
  Serial.print("Nueva clave dinámica generada: ");
  Serial.println(clave_cloud);
}
//---------------------------------------------------------------------------------
bool verificar_password(const char *password, String &nombreUsuario) {
    if (password == nullptr || strlen(password) == 0) {
        Serial.println("[AUTH] Error: Contraseña vacía");
        return false;
    }

    String inputStr(password);
    inputStr.trim();
    uint16_t claveIngresada = atoi(inputStr.c_str());

    Serial.print("[AUTH] Verificando clave: ");
    Serial.println(claveIngresada);

    // 1. Primero verificar accesos personalizados (tienen prioridad)
    for(int i = 0; i < totalAccesosPersonalizados; i++) {
        if(accesosPersonalizados[i].clave == claveIngresada) {
            String fechaActual = obtenerFechaActualFormateada();
            int cmpInicio = compararFechas(fechaActual.c_str(), accesosPersonalizados[i].fechaInicio);
            int cmpFin = compararFechas(fechaActual.c_str(), accesosPersonalizados[i].fechaFin);
           
            if(cmpInicio >= 0 && cmpFin <= 0) {
                nombreUsuario = String(accesosPersonalizados[i].nombre);
                Serial.print("[AUTH] Acceso personalizado válido para: ");
                Serial.println(nombreUsuario);
                return true;
            } else {
                Serial.println("[AUTH] Acceso personalizado fuera de fecha válida");
                return false;
            }
        }
    }

    // 2. Verificar clave dinámica
    if (claveIngresada == clave_cloud) {
        Serial.println("[AUTH] Clave dinámica válida");
        nombreUsuario = "Acceso Dinámico";
        return true;
    }

    // 3. Verificar clave por defecto
    if (inputStr.equals(default_password)) {
        Serial.println("[AUTH] Clave por defecto válida");
        nombreUsuario = "Acceso Default";
        return true;
    }

    Serial.println("[AUTH] Clave no reconocida");
    return false;
}
//---------------------------------------------------------------------------------

void checkPasswordUpdate() {
  if (updatePasswordFlag) {
    updatePasswordFlag = false; // Resetear bandera
    generateNewPassword();
  }
}


//---------CREAR PANTALLA---------
void crear_pantalla_principal() {
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), LV_PART_MAIN);
  main_container = lv_obj_create(main_screen);
  lv_obj_set_size(main_container, Display.width(), Display.height());
  lv_obj_set_style_bg_color(main_container, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_border_color(main_container, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
  lv_obj_center(main_container);
  crear_iconos_estado();
  crear_elementos_fecha_hora();
  crear_botones_principales();
}
void crear_iconos_estado() {
  lv_obj_t *status_container_left = lv_obj_create(main_container);
  lv_obj_remove_style_all(status_container_left);
  lv_obj_set_size(status_container_left, 80, 60);
  lv_obj_align(status_container_left, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_set_flex_flow(status_container_left, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_border_width(status_container_left, 0, LV_PART_MAIN);
  bat_label = lv_label_create(status_container_left);
  lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_obj_set_style_text_color(bat_label, lv_color_white(), LV_PART_MAIN);
  lv_obj_t *status_container_right = lv_obj_create(main_container);
  lv_obj_remove_style_all(status_container_right);
  lv_obj_set_size(status_container_right, 40, 40);
  lv_obj_align(status_container_right, LV_ALIGN_TOP_RIGHT, -10, 10);
  wifi_label = lv_label_create(status_container_right);
  lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_32, LV_PART_MAIN);
  lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
}
void crear_elementos_fecha_hora() {
  // 1. Crear el contenedor principal para la fecha y la hora
  dateTimeContainer = lv_obj_create(main_container);
  lv_obj_remove_style_all(dateTimeContainer);                            // Quita estilos por defecto (como padding) del objeto base
  lv_obj_set_size(dateTimeContainer, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // El ancho y alto se adaptarán al contenido

  // 2. Alinear este contenedor agrupador en la parte superior central de main_container
  // La posición vertical (85) se aplica a este contenedor.
  lv_obj_align(dateTimeContainer, LV_ALIGN_TOP_MID, 0, 85);

  // 3. Configurar Flexbox para el dateTimeContainer
  // Esto apilará los elementos hijos (hora y fecha) verticalmente.
  lv_obj_set_flex_flow(dateTimeContainer, LV_FLEX_FLOW_COLUMN);
  // Esto centrará los elementos hijos horizontalmente dentro de dateTimeContainer.
  lv_obj_set_flex_align(dateTimeContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  // Espacio opcional entre la hora y la fecha
  lv_obj_set_style_pad_row(dateTimeContainer, 10, 0);  // 10px de espacio, ajusta si es necesario


  // --- Hora ---
  // lbl_hora ahora es hijo de dateTimeContainer
  lbl_hora = lv_label_create(dateTimeContainer);
  lv_obj_set_style_text_font(lbl_hora, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(lbl_hora, lv_color_white(), 0);
  lv_label_set_text(lbl_hora, "00:00 AM");  // Texto inicial de placeholder

  // Asegurar que el texto DENTRO de la etiqueta esté centrado
  // (Importante si la etiqueta tiene padding, como el del borde)
  lv_obj_set_style_text_align(lbl_hora, LV_TEXT_ALIGN_CENTER, 0);

  // Aplicar el borde inferior y padding directamente a la etiqueta de la hora
  lv_obj_set_style_border_color(lbl_hora, lv_color_white(), 0);
  lv_obj_set_style_border_width(lbl_hora, 2, 0);
  lv_obj_set_style_border_side(lbl_hora, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_pad_bottom(lbl_hora, 6, 0);  // Padding para que el texto no toque el borde


  // --- Fecha ---
  // lbl_fecha ahora es hijo de dateTimeContainer
  lbl_fecha = lv_label_create(dateTimeContainer);
  lv_obj_set_style_text_font(lbl_fecha, &lv_font_montserrat_34, 0);
  lv_obj_set_style_text_color(lbl_fecha, lv_color_white(), 0);
  lv_label_set_text(lbl_fecha, "Dia, DD de Mes");  // Texto inicial de placeholder

  // Asegurar que el texto DENTRO de la etiqueta esté centrado
  lv_obj_set_style_text_align(lbl_fecha, LV_TEXT_ALIGN_CENTER, 0);

  // Con Flexbox en dateTimeContainer, las etiquetas lbl_hora y lbl_fecha
  // se colocarán y alinearán automáticamente según la configuración de flex.
  // No se necesita lv_obj_align() ni lv_obj_align_to() para ellas aquí.
}
void crear_botones_principales() {
  btn_timbre = crear_boton_basico(main_container, 440, 110, "TIMBRE", &lv_font_montserrat_48, on_timbre_click);
  lv_obj_align(btn_timbre, LV_ALIGN_BOTTOM_MID, 0, -360);

  btn_ingreso = crear_boton_basico(main_container, 440, 110, "INGRESO", &lv_font_montserrat_48, on_ingreso_click);
  lv_obj_align(btn_ingreso, LV_ALIGN_BOTTOM_MID, 0, -240);

  btn_salida = crear_boton_basico(main_container, 440, 110, "SALIDA", &lv_font_montserrat_48, on_salida_click);
  lv_obj_align(btn_salida, LV_ALIGN_BOTTOM_MID, 0, -120);

  btn_clave = crear_boton_basico(main_container, 440, 110, "CLAVE", &lv_font_montserrat_48, on_clave_click);
  lv_obj_align(btn_clave, LV_ALIGN_BOTTOM_MID, 0, 0);
}
void crear_pantalla_teclado() {
  lv_obj_set_style_bg_color(keypad_screen, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_t *keypad_container = lv_obj_create(keypad_screen);
  lv_obj_set_size(keypad_container, Display.width(), Display.height());
  lv_obj_set_style_bg_color(keypad_container, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_center(keypad_container);
  lv_obj_t *btn_back = crear_boton_basico(keypad_container, 160, 80, LV_SYMBOL_LEFT " Volver", &lv_font_montserrat_28, on_back_click);
  lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 10, 10);
  ta_password = lv_textarea_create(keypad_container);
  lv_textarea_set_text(ta_password, "");
  lv_textarea_set_placeholder_text(ta_password, "Ingrese la clave");
  lv_textarea_set_password_mode(ta_password, true);
  lv_obj_set_size(ta_password, 450, 80);
  lv_obj_align(ta_password, LV_ALIGN_TOP_MID, 0, 120);
  lv_obj_set_style_text_font(ta_password, &lv_font_montserrat_48, LV_PART_MAIN);
  static const char *btnm_map[] = {
    "1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n",
    LV_SYMBOL_CLOSE, "0", LV_SYMBOL_BACKSPACE, "\n", LV_SYMBOL_REFRESH, LV_SYMBOL_OK, ""
  };
  keypad = lv_btnmatrix_create(keypad_container);
  lv_btnmatrix_set_map(keypad, btnm_map);
  lv_obj_set_size(keypad, 480, 400);
  lv_obj_align(keypad, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(keypad, on_keypad_click, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_style_text_font(keypad, &lv_font_montserrat_32, LV_PART_ITEMS);
}
lv_obj_t *crear_boton_basico(lv_obj_t *parent, int width, int height, const char *texto, const lv_font_t *fuente, lv_event_cb_t callback) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, texto);
  lv_obj_set_style_text_font(label, fuente, LV_PART_MAIN);
  lv_obj_center(label);
  if (callback != NULL) lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, NULL);
  return btn;
}
void cambiar_a_pantalla(lv_obj_t *screen) {
  limpiar_campos_texto();
  lv_scr_load(screen);
}
void mostrar_teclado_numerico(lv_obj_t *ta_input) {
    cambiar_a_pantalla(keypad_screen);
}
void limpiar_campos_texto() {
  if (ta_password != NULL) lv_textarea_set_text(ta_password, "");
}

//Funciones para mostrar mensajes en pantalla
void mostrarPopup(const char *mensaje, const char *simbolo_icono, uint32_t duracion_segundos) {
  lv_obj_t *current_screen = lv_scr_act();  // Obtener la pantalla activa

  // Crear el contenedor del pop-up
  lv_obj_t *popup_container = lv_obj_create(current_screen);
  lv_obj_set_size(popup_container, 460, 460);
  lv_obj_center(popup_container);
  // Estilos del contenedor (los mismos que el "ingreso exitoso" original)
  lv_obj_set_style_bg_color(popup_container, lv_color_hex(0x2c3e50), 0);
  lv_obj_set_style_bg_opa(popup_container, LV_OPA_90, 0);
  lv_obj_set_style_border_color(popup_container, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(popup_container, 2, 0);
  lv_obj_set_style_radius(popup_container, 15, 0);

  // Configurar layout flexible para centrar contenido
  lv_obj_set_flex_flow(popup_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(popup_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(popup_container, 25, 0);
  lv_obj_set_style_pad_row(popup_container, 20, 0);  // Espacio entre icono y texto

  // Crear etiqueta para el icono
  lv_obj_t *icon_label = lv_label_create(popup_container);
  lv_label_set_text(icon_label, simbolo_icono);  // Usar el icono pasado como parámetro
  lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);
  // Color del icono (verde por defecto, como en "ingreso exitoso")
  // Si se quisiera un color dinámico, se pasaría como parámetro a mostrarPopup
  if (strcmp(simbolo_icono, LV_SYMBOL_OK) == 0) {  // Ejemplo: verde para OK
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x2ecc71), 0);
  } else if (strcmp(simbolo_icono, LV_SYMBOL_CLOSE) == 0 || strcmp(simbolo_icono, LV_SYMBOL_WARNING) == 0) {  // Rojo para error/advertencia
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFF0000), 0);
  } else {  // Color por defecto para otros iconos
    lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
  }

  // Crear etiqueta para el texto del mensaje
  lv_obj_t *text_label = lv_label_create(popup_container);
  lv_label_set_text(text_label, mensaje);  // Usar el mensaje pasado como parámetro
  lv_obj_set_style_text_font(text_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(text_label, lv_color_white(), 0);  // Texto en blanco

  // Crear timer para cerrar el pop-up
  // Usar duracion_segundos para el timer (convertido a milisegundos)
  // y el nuevo callback genérico
  lv_timer_t *popup_timer = lv_timer_create(eliminar_popup_timer_cb, duracion_segundos * 1000, popup_container);
  lv_timer_set_repeat_count(popup_timer, 1);  // Ejecutar solo una vez
}
void mostrarPopupConfirmacionUsuario(const DatosUsuario *usuario) {
  // Eliminar pop-up anterior si existe
  if (confirm_popup && lv_obj_is_valid(confirm_popup)) {
    lv_obj_del(confirm_popup);
  }

  // Crear contenedor principal
  confirm_popup = lv_obj_create(lv_scr_act());
  lv_obj_set_size(confirm_popup, 700, 460);
  lv_obj_center(confirm_popup);
  lv_obj_set_style_bg_color(confirm_popup, lv_color_hex(0x2c3e50), 0);
  lv_obj_set_style_border_color(confirm_popup, lv_color_white(), 0);
  lv_obj_set_style_border_width(confirm_popup, 3, 0);
  lv_obj_set_style_radius(confirm_popup, 15, 0);
  lv_obj_set_flex_flow(confirm_popup, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(confirm_popup, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(confirm_popup, 30, 0);
  lv_obj_set_style_pad_row(confirm_popup, 20, 0);

  // Título
  lv_obj_t *title = lv_label_create(confirm_popup);
  lv_label_set_text(title, "CONFIRMAR DATOS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  // Mostrar datos del usuario
  char info_buffer[256];
  snprintf(info_buffer, sizeof(info_buffer),
           "%s\n"
           "ID: %s\n"
           "Cel: %s\n"
           "Nac: %s\n",
           usuario->nombre,
           usuario->documento,
           (usuario->telefono[0] != '\0') ? usuario->telefono : "N/A",
           (usuario->fechaNacimiento[0] != '\0') ? usuario->fechaNacimiento : "No registrada",
           tempIdHuellaHolder);

  lv_obj_t *data_label = lv_label_create(confirm_popup);
  lv_label_set_text(data_label, info_buffer);
  lv_obj_set_style_text_font(data_label, &lv_font_montserrat_38, 0);
  lv_obj_set_style_text_color(data_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(data_label, LV_TEXT_ALIGN_LEFT, 0);

  // Contenedor para botones
  lv_obj_t *btn_container = lv_obj_create(confirm_popup);
  lv_obj_remove_style_all(btn_container);
  lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Botón Confirmar (OK)
  btn_confirm_ok = lv_btn_create(btn_container);
  lv_obj_set_size(btn_confirm_ok, 120, 80);
  lv_obj_t *lbl_ok = lv_label_create(btn_confirm_ok);
  lv_label_set_text(lbl_ok, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_38, 0);
  lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0x00FF00), 0);
  lv_obj_add_event_cb(btn_confirm_ok, on_confirmar_datos_ok, LV_EVENT_CLICKED, NULL);

  // Botón Cancelar (X)
  btn_confirm_cancel = lv_btn_create(btn_container);
  lv_obj_set_size(btn_confirm_cancel, 120, 80);
  lv_obj_t *lbl_cancel = lv_label_create(btn_confirm_cancel);
  lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_38, 0);
  lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0xFF0000), 0);
  lv_obj_add_event_cb(btn_confirm_cancel, on_confirmar_datos_cancel, LV_EVENT_CLICKED, NULL);

  reproducirPistaAudio(AUDIO_CONFIRT_DATA);
}
static void eliminar_popup_timer_cb(lv_timer_t *timer) {
  lv_obj_t *popup = (lv_obj_t *)timer->user_data;
  if (popup && lv_obj_is_valid(popup)) {  // Buena práctica: verificar si el objeto aún es válido
    lv_obj_del(popup);
  }
}
void animar_pulsacion_boton(lv_obj_t *btn, uint32_t color_original_hex_no_usado) {
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, btn);
  lv_anim_set_time(&a, 150);
  lv_anim_set_delay(&a, 50);
  lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
    lv_obj_set_style_bg_color((lv_obj_t *)var, lv_color_hex(0x000000), LV_PART_MAIN);
  });
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_set_values(&a, 0, 1);
  lv_anim_start(&a);
}



//----------Actualizaciones de iconos----
void actualizarIconoBateriaLVGL(int nivel) {
  if (bat_label == NULL) return;
  const char *bat_symbol = LV_SYMBOL_BATTERY_FULL;
  if (nivel <= 10) bat_symbol = LV_SYMBOL_BATTERY_EMPTY;
  else if (nivel <= 30) bat_symbol = LV_SYMBOL_BATTERY_1;
  else if (nivel <= 60) bat_symbol = LV_SYMBOL_BATTERY_2;
  else if (nivel <= 90) bat_symbol = LV_SYMBOL_BATTERY_3;
  lv_label_set_text(bat_label, bat_symbol);
}
void actualizarColorIconoWifiLVGL(int status) {
  if (wifi_label != NULL) {
    lv_color_t color;
    switch (status) {
      case 1: color = lv_color_hex(0x00FF00); break;
      case 2: color = lv_color_hex(0xFF0000); break;
      case 3: color = lv_color_hex(0x808080); break;
      default: color = lv_color_hex(0x808080); break;
    }
    lv_obj_set_style_text_color(wifi_label, color, LV_PART_MAIN);
  }
}


//-------------EVENTOS TOUCH----------------
static void on_timbre_click(lv_event_t *e) {
  animar_pulsacion_boton(lv_event_get_target(e), 0);
  reproducirPistaAudio(AUDIO_TIMBRE);
  mostrarPopup("Timbre...", LV_SYMBOL_BELL, 3);
  String mensajeNotificacion = "Alguien en la puerta ";
  sendNotification(mensajeNotificacion);
}
static void on_ingreso_click(lv_event_t *e) {
  animar_pulsacion_boton(lv_event_get_target(e), 0);

  // Cancelar cualquier estado previo
  apagarTodosLEDs();

  // Activar nuevo estado
  bandera_ingreso_activo = true;
  ingreso_timer.attach(desactivar_ingreso_flag, 7.0f);

  // Configurar LEDs
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE, 0);
  accesorios_led.setColor(RGBLed::BLUE);

  mostrarPopup("-------INGRESO-------", LV_SYMBOL_PLAY, 3);
}
static void on_salida_click(lv_event_t *e) {
  animar_pulsacion_boton(lv_event_get_target(e), 0);

  // Cancelar cualquier estado previo
  apagarTodosLEDs();

  // Activar nuevo estado
  bandera_salida_activo = true;
  salida_timer.attach(desactivar_salida_flag, 7.0f);

  // Configurar LEDs
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_RED, 0);
  accesorios_led.setColor(RGBLed::RED);

  mostrarPopup("-------SALIDA-------", LV_SYMBOL_NEXT, 2);
}
static void on_clave_click(lv_event_t *e) {
    animar_pulsacion_boton(lv_event_get_target(e), 0);
   
    // Mostrar información de la clave dinámica actual
    unsigned long remainingTime = PASSWORD_UPDATE_INTERVAL - (millis() - lastPasswordUpdate);
    int hours = remainingTime / 3600000;
    int minutes = (remainingTime % 3600000) / 60000;
   
    char infoMsg[80];
    snprintf(infoMsg, sizeof(infoMsg), "Clave dinámica actual: %04d\nVálida por: %02d:%02d\n\nIngrese cualquier clave válida",
             (int)clave_cloud, hours, minutes);
             
    //mostrarPopup(infoMsg, LV_SYMBOL_OK, 3);
    mostrar_teclado_numerico(ta_password);
}
static void on_back_click(lv_event_t *e) {
  cambiar_a_pantalla(main_screen);
}
static void on_keypad_click(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    const char *txt = lv_btnmatrix_get_btn_text(obj, id);

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_del_char(ta_password);
    }
    else if (strcmp(txt, LV_SYMBOL_CLOSE) == 0) {
        cambiar_a_pantalla(main_screen);
    }
    else if (strcmp(txt, LV_SYMBOL_REFRESH) == 0) {
        lv_textarea_set_text(ta_password, "");
    }
    else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        const char *password_ingresado = lv_textarea_get_text(ta_password);
        String nombreUsuario;
       
        if (verificar_password(password_ingresado, nombreUsuario)) {
            // Acceso concedido
            String mensaje = "Acceso\n";
            if (!nombreUsuario.isEmpty()) {
                mensaje += nombreUsuario + "\n";
            }
            mensaje += "Clave: " + String(password_ingresado);
           
            //mostrarPopup(mensaje.c_str(), LV_SYMBOL_OK, 5);
            //mostrarPopup(mensaje.c_str(), LV_SYMBOL_OK, 5);
            reproducirPistaAudio(CLAVE_CORRECTA);
           
            // Enviar notificación a la nube
            String notificacion = "ACCESO (CLAVE):\nNombre: " + nombreUsuario +
                    "\nClave: " + String(password_ingresado) +
                    "\nFecha: " + obtenerFechaActualFormateada();
            sendNotification(notificacion);
        } else {
            mostrarPopup("Clave incorrecta", LV_SYMBOL_WARNING, 3);
            reproducirPistaAudio(CLAVE_INCORRECTA);
        }
       
        lv_textarea_set_text(ta_password, "");
        cambiar_a_pantalla(main_screen);
    }
    else {
        lv_textarea_add_text(ta_password, txt);
    }
}
//confirmacion de informacion de los datos del usuario enviados desde cloud
static void on_confirmar_datos_ok(lv_event_t *e) {
  if (confirm_popup) {
    lv_obj_del(confirm_popup);
    confirm_popup = NULL;
  }

  // Reproducir audio de instrucción para colocar el dedo
  mostrarPopup("Registrar Huella", LV_SYMBOL_OK, 3);
  delay(1000);
  reproducirPistaAudio(AUDIO_COLOQUE_DEDO);

  // Continuar con el registro de huella
  String msg = "Datos confirmados. ID de Huella asignado: " + String(tempIdHuellaHolder) + ".\n";
  msg += "Por favor, siga las indicaciones para el registro de la huella.\n";
  enviarMensajeChat(msg, false);
  currentSystemState = ADD_USER_INITIATE_FINGERPRINT;
}
static void on_confirmar_datos_cancel(lv_event_t *e) {
  if (confirm_popup) {
    lv_obj_del(confirm_popup);
    confirm_popup = NULL;
  }

  resetSessionCloud("Registro cancelado. Por favor inicie nuevamente si desea registrar un usuario.");
}



// ===== FUNCIONES DE ARDUINO CLOUD Y CHAT =====

void inicializarDispositivosAcceso() {
  Serial.println(F("\n--- Inicializando Dispositivos de Acceso ---"));
  inicializarLEDsAccesorios();
  inicializarAudio();

  // ===== CORRECCION ERROR COMPILACION INICIO =====
  sensorHuellasInicializadoOK = inicializarSensorHuellasR503();  // Usar la nueva variable global
  usbConnected = inicializarConexionUSB();

  if (!sensorHuellasInicializadoOK) {  // Usar la nueva variable global
                                       // ===== CORRECCION ERROR COMPILACION FIN =====
    Serial.println(F("ERROR CRITICO: Sensor de huellas no disponible. Funcionalidad limitada."));
    if (ArduinoCloud.connected()) enviarMensajeChat("ERROR: Sensor de huellas no detectado. Funciones de acceso deshabilitadas.");
  } else {
    attachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN), ISR_DedoDetectado, FALLING);
    Serial.println(F("Sensor de huellas listo. Deteccion por Wake-Up activada."));
  }
  Serial.println(F("--- Fin Inicializacion Dispositivos ---"));
}
void setActualizarHoraFlagCloud() {
  actualizarHoraFlag = true;
}
void actualizarYAlmacenarHoraFormateadaCloud() {
  if (!ArduinoCloud.connected()) {
    strcpy(globalFormattedDate, "Cloud Desconectado");
    strcpy(globalFormattedTime, "Hora No Disp.");
    return;
  }
  time_t currentLocalEpoch = ArduinoCloud.getLocalTime();
  if (currentLocalEpoch == 0 || currentLocalEpoch < 1600000000L) {
    strcpy(globalFormattedDate, "Tiempo No Sinc.");
    strcpy(globalFormattedTime, "Hora No Sinc.");
    return;
  }

  struct tm *timeinfo = localtime(&currentLocalEpoch);
  if (timeinfo) {
    sprintf(globalFormattedDate, "%s, %d de %s",
            nombresDias[timeinfo->tm_wday],
            timeinfo->tm_mday,
            nombresMeses[timeinfo->tm_mon]);

    int hour12 = timeinfo->tm_hour;
    const char *ampm = "am";
    if (hour12 == 0) {
      hour12 = 12;
    } else if (timeinfo->tm_hour == 12) {
      ampm = "pm";
    } else if (timeinfo->tm_hour > 12) {
      hour12 -= 12;
      ampm = "pm";
    }
    sprintf(globalFormattedTime, "%d:%02d %s", hour12, timeinfo->tm_min, ampm);
  } else {
    strcpy(globalFormattedDate, "Error Formato Fecha");
    strcpy(globalFormattedTime, "Error Formato Hora");
  }

  if (lbl_hora != NULL) {
    lv_label_set_text(lbl_hora, globalFormattedTime);
  }
  if (lbl_fecha != NULL) {
    lv_label_set_text(lbl_fecha, globalFormattedDate);
  }
}
void enviarMensajeChat(const String &mensaje, bool appendDefaultPrompt) {
  String finalMessage = mensaje;
  if (appendDefaultPrompt && (currentSystemState == IDLE || currentSystemState == MAIN_MENU_AWAITING_CHOICE)) {
  } else if (appendDefaultPrompt) {
    finalMessage += "\nEscribe 'menu' para volver al inicio o 'cancelar' para anular la operacion actual.";
  }
  chat = finalMessage;
  Serial.print(F("Enviando a Cloud Chat: "));
  Serial.println(finalMessage);
}
void onLedChange() {
}



void onChatChange() {
    String mensajeRecibido = chat;
    mensajeRecibido.trim();

    Serial.print(F("CHAT: Mensaje recibido '"));
    Serial.print(mensajeRecibido);
    Serial.println(F("'"));

    if (!ArduinoCloud.connected()) {
        Serial.println(F("CHAT: No conectado a Arduino Cloud. Ignorando mensaje."));
        return;
    }

    lastInteractionTimeCloud = millis();

    String commandCheck = mensajeRecibido;
    commandCheck.toLowerCase();

    if (commandCheck.equals("cancelar") && currentSystemState != IDLE &&
        currentSystemState != WAKEUP_PROCESSING_FINGERPRINT &&
        currentSystemState != WAKEUP_WAITING_FOR_FINGER_REMOVAL &&
        currentSystemState != ADD_USER_PROCESSING_FINGERPRINT &&
        currentSystemState != DELETE_USER_PROCESSING) {
        resetSessionCloud(MSG_CANCELLED_BY_USER);
        return;
    }
   
    if (commandCheck.equals("ayuda")) {
        enviarMensajeChat(MSG_INITIATE_MENU_PROMPT);
        if (currentSystemState != IDLE) resetSessionCloud();
        return;
    }

    switch (currentSystemState) {
        case IDLE:
            if (commandCheck.equals("menu") || commandCheck.startsWith("hola")) {
                mostrarMenuPrincipalCloud();
            } else if (!mensajeRecibido.isEmpty()) {
                enviarMensajeChat(MSG_INITIATE_MENU_PROMPT);
            }
            break;

        case MAIN_MENU_AWAITING_CHOICE:
            procesarOpcionMenuPrincipalCloud(mensajeRecibido);
            break;

        case ADD_USER_AWAITING_NAME:
            procesarNombreUsuarioCloud(mensajeRecibido);
            break;
           
        case ADD_USER_AWAITING_DOCUMENT:
            procesarDocumentoUsuarioCloud(mensajeRecibido);
            break;
           
        case ADD_USER_AWAITING_PHONE:
            procesarTelefonoUsuarioCloud(mensajeRecibido);
            enviarMensajeChat("Para continuar con el registro de huella, escribe 'ok' o 'listo'. Tambien puedes colocar tu dedo en el sensor. Para anular, escribe 'cancelar'.", false);
            break;
           
        case ADD_USER_INITIATE_FINGERPRINT:
            if (commandCheck.equals("ok") || commandCheck.equals("siguiente") || commandCheck.equals("listo")) {
                mostrarPopup("Registrando Nuevo\n Usuario", LV_SYMBOL_PLUS, 3);
                reproducirPistaAudio(AUDIO_USER_ADD);
                ejecutarRegistroHuellaCloud();
            } else {
                enviarMensajeChat("Para continuar con el registro de huella, escribe 'ok' o 'listo'. Tambien puedes colocar tu dedo en el sensor. Para anular, escribe 'cancelar'.", false);
                mostrarPopup("Registrando Nuevo\n Usuario", LV_SYMBOL_PLUS, 3);
                reproducirPistaAudio(AUDIO_USER_ADD);
            }
            break;

        case AWAITING_ACCESS_NAME:
            procesarNombreAccesoPersonalizado(mensajeRecibido);
            break;
           
        case AWAITING_ACCESS_START_DATE:
            procesarFechaInicioAcceso(mensajeRecibido);
            break;
           
        case AWAITING_ACCESS_END_DATE:
            procesarFechaFinAcceso(mensajeRecibido);
            break;

        case DELETE_USER_AWAITING_IDENTIFIER:
            procesarIdentificadorParaEliminarCloud(mensajeRecibido);
            break;
           
        case DELETE_USER_CONFIRMING:
            confirmarYEliminarUsuarioCloud(mensajeRecibido);
            break;

        case ADJUST_VOLUME_AWAITING_LEVEL:
            procesarNuevoVolumenCloud(mensajeRecibido);
            break;
       
        case ADD_USER_AWAITING_BIRTHDATE:
            procesarFechaNacimientoCloud(mensajeRecibido);
            break;

        case ADD_USER_CONFIRMING_DATA:
            break;

        default:
            Serial.print(F("CHAT: Estado de menu desconocido: "));
            Serial.println(currentSystemState);
            resetSessionCloud(MSG_GENERAL_ERROR_CLOUD);
            break;
    }
}
void resetSessionCloud(const String &previousMessage) {
  if (!previousMessage.isEmpty()) {
    enviarMensajeChat(previousMessage, false);
  }
  currentSystemState = IDLE;
  memset(&tempUserHolder, 0, sizeof(DatosUsuario));
  tempIdHuellaHolder = 0;
  memset(tempDocumentoHolder, 0, sizeof(tempDocumentoHolder));

  if (procesandoHuellaActual) {
    apagarTodosLEDs();
    attachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN), ISR_DedoDetectado, FALLING);
    procesandoHuellaActual = false;
  }
  Serial.println(F("CHAT: Sesion reiniciada. Estado: IDLE."));
}
void mostrarMenuPrincipalCloud() {
  String menu = "Menu Principal:\n";
  menu += "1. Registrar Usuario\n";
  menu += "2. Eliminar Usuario\n";
  menu += "3. Ver Usuarios\n";
  menu += "4. Ajustar Volumen\n";
  menu += "5. Acceso Personalizado\n\n";
  menu += "Escribe el numero de la opcion o 'cancelar'.";
 
  enviarMensajeChat(menu, false);
  currentSystemState = MAIN_MENU_AWAITING_CHOICE;
  Serial.println(F("CHAT: Mostrando menu principal. Estado: MAIN_MENU_AWAITING_CHOICE."));
}
void procesarOpcionMenuPrincipalCloud(const String &opcion) {
  if (opcion == "1") {
    iniciarRegistroUsuarioCloud();
  } else if (opcion == "2") {
    iniciarEliminacionUsuarioCloud();
  } else if (opcion == "3") {
    mostrarUsuariosCloud();
  } else if (opcion == "4") {
    iniciarAjusteVolumenCloud();
  } else if (opcion == "5") {
    iniciarFlujoAccesoPersonalizado();
  } else {
    enviarMensajeChat(MSG_INVALID_OPTION_MENU, false);
    Serial.println(F("CHAT: Opcion invalida recibida en menu principal."));
  }
}
void iniciarRegistroUsuarioCloud() {
  Serial.println(F("CHAT: Iniciando flujo de Registro de Usuario."));
  if (!verificarConexionUSB()) {
    resetSessionCloud(MSG_USB_NOT_CONNECTED);
    return;
  }
  if (totalUsuariosAlmacenados >= MAX_USUARIOS_EN_MEMORIA) {
    resetSessionCloud(MSG_MAX_USERS_REACHED);
    return;
  }
  memset(&tempUserHolder, 0, sizeof(DatosUsuario));
  tempUserHolder.activo = true;
  enviarMensajeChat("Registrando nuevo usuario...\nIngrese Nombre Completo (max " + String(NOMBRE_MAX_LENGTH - 1) + " chars):", false);
  currentSystemState = ADD_USER_AWAITING_NAME;
}
void procesarNombreUsuarioCloud(const String &nombre) {
  if (nombre.isEmpty() || nombre.length() >= NOMBRE_MAX_LENGTH) {
    enviarMensajeChat("Nombre invalido o muy largo. Intente de nuevo (max " + String(NOMBRE_MAX_LENGTH - 1) + " chars):", false);
    return;
  }
  strcpy(tempUserHolder.nombre, nombre.c_str());
  Serial.print(F("CHAT: Nombre recibido: "));
  Serial.println(tempUserHolder.nombre);
  enviarMensajeChat("Ingrese Documento de Identidad (Cedula, max " + String(DOCUMENTO_MAX_LENGTH - 1) + " digitos, numerico):", false);
  currentSystemState = ADD_USER_AWAITING_DOCUMENT;
}
void procesarDocumentoUsuarioCloud(const String &documento) {
  bool esValido = !documento.isEmpty() && documento.length() < DOCUMENTO_MAX_LENGTH;
  for (unsigned int i = 0; i < documento.length(); i++) {
    if (!isdigit(documento.charAt(i))) {
      esValido = false;
      break;
    }
  }

  if (!esValido) {
    enviarMensajeChat("Documento invalido (debe ser numerico, no vacio y max " + String(DOCUMENTO_MAX_LENGTH - 1) + " digitos). Intente de nuevo:", false);
    return;
  }
  if (buscarIndiceUsuarioPorDocumento(documento.c_str()) != -1) {
    enviarMensajeChat("Error: Ya existe un usuario registrado con ese Documento de Identidad. Intente con otro o cancele.", false);
    return;
  }

  strcpy(tempUserHolder.documento, documento.c_str());
  Serial.print(F("CHAT: Documento recibido: "));
  Serial.println(tempUserHolder.documento);
  enviarMensajeChat("Ingrese Numero de Celular (max " + String(TELEFONO_MAX_LENGTH - 1) + " digitos, numerico, opcional - presione Enter o envie '.' si no desea):", false);
  currentSystemState = ADD_USER_AWAITING_PHONE;
}
void procesarTelefonoUsuarioCloud(const String &telefono) {
  if (telefono.equals(".") || telefono.isEmpty()) {
    tempUserHolder.telefono[0] = '\0';
    Serial.println(F("CHAT: Telefono omitido."));
  } else {
    bool esNumerico = true;
    if (telefono.length() >= TELEFONO_MAX_LENGTH) esNumerico = false;
    for (unsigned int i = 0; i < telefono.length(); i++) {
      if (!isdigit(telefono.charAt(i))) {
        esNumerico = false;
        break;
      }
    }
    if (!esNumerico) {
      enviarMensajeChat("Telefono invalido (debe ser numerico y corto). Intente de nuevo o envie '.' para omitir:", false);
      return;
    }
    strcpy(tempUserHolder.telefono, telefono.c_str());
    Serial.print(F("CHAT: Telefono recibido: "));
    Serial.println(tempUserHolder.telefono);
  }

  // Cambio importante: Ahora pedimos fecha de nacimiento, no vamos directo a huella
  enviarMensajeChat("Ingrese Fecha de Nacimiento (dd/mm/aaaa):", false);
  currentSystemState = ADD_USER_AWAITING_BIRTHDATE;
}
void ejecutarRegistroHuellaCloud() {
  if (currentSystemState != ADD_USER_INITIATE_FINGERPRINT && currentSystemState != ADD_USER_PROCESSING_FINGERPRINT) {
    if (currentSystemState == ADD_USER_PROCESSING_FINGERPRINT) return;
  }

  detachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN));
  procesandoHuellaActual = true;
  //verificar correcion
  mostrarPopup("Registrando Nuevo\n Usuario", LV_SYMBOL_PLUS, 3);
  //reproducirPistaAudio(AUDIO_USER_ADD);

  enviarMensajeChat("Iniciando captura de huella... Siga las indicaciones del dispositivo (LED/Audio). Esto tomara unos segundos. NO ESCRIBA NADA EN EL CHAT HASTA QUE TERMINE.", false);
  currentSystemState = ADD_USER_PROCESSING_FINGERPRINT;

  ArduinoCloud.update();
  delay(100);

  bool exitoHuella = false;
  for (int i = 0; i < MAX_REINTENTOS_REGISTRO; ++i) {
    if (registrarNuevaHuellaParaUsuario(tempIdHuellaHolder)) {
      exitoHuella = true;
      break;
    }
    if (i < MAX_REINTENTOS_REGISTRO - 1) {
      enviarMensajeChat("Intento de registro de huella " + String(i + 1) + " fallido. Reintentando en unos segundos...", false);
      ArduinoCloud.update();
      delay(100);
      delay(2000);
    }
  }
  finalizarRegistroUsuarioCloud(exitoHuella);

  attachInterrupt(digitalPinToInterrupt(FINGER_DETECT_PIN), ISR_DedoDetectado, FALLING);
  procesandoHuellaActual = false;
  dedoDetectadoFlag = false;
}
void finalizarRegistroUsuarioCloud(bool exitoHuella) {
  if (exitoHuella) {
    tempUserHolder.idHuella = tempIdHuellaHolder;
    usuarios[totalUsuariosAlmacenados] = tempUserHolder;
    totalUsuariosAlmacenados++;

    if (guardarUsuariosEnUSB()) {
      String msg = "USUARIO REGISTRADO CON EXITO!\n";
      msg += "Nombre: " + String(tempUserHolder.nombre) + "\n";
      msg += "ID Huella asignado: " + String(tempUserHolder.idHuella);
     
      // Enviar notificación de registro
      String notificacion = "REGISTRO:\nNombre: " + String(tempUserHolder.nombre) +
                    "\nDocumento: " + String(tempUserHolder.documento) +
                    "\nTeléfono: " + String((tempUserHolder.telefono[0] != '\0') ? tempUserHolder.telefono : "N/A") +
                    "\nNacimiento: " + String(tempUserHolder.fechaNacimiento) +
                    "\nID Huella: " + String(tempIdHuellaHolder);
      sendNotification(notificacion);
     
      resetSessionCloud(msg);
      reproducirPistaAudio(AUDIO_USER_ADD);
    } else {
      totalUsuariosAlmacenados--;
      finger.deleteModel(tempIdHuellaHolder);
      resetSessionCloud("Error CRITICO: Usuario registrado en memoria pero fallo el guardado en USB. Huella eliminada del sensor.");
      reproducirPistaAudio(AUDIO_ERROR);
    }
  } else {
    resetSessionCloud("FALLO el registro de la huella dactilar. Usuario no registrado.");
    reproducirPistaAudio(AUDIO_ERROR);
  }
}
void iniciarEliminacionUsuarioCloud() {
  Serial.println(F("CHAT: Iniciando flujo de Eliminacion de Usuario."));
  if (!verificarConexionUSB()) {
    resetSessionCloud(MSG_USB_NOT_CONNECTED);
    return;
  }
  if (totalUsuariosAlmacenados == 0) {
    resetSessionCloud("No hay usuarios registrados para eliminar.");
    return;
  }
  enviarMensajeChat("Ingrese el numero de Cedula del usuario a eliminar:", false);
  currentSystemState = DELETE_USER_AWAITING_IDENTIFIER;
}
void procesarIdentificadorParaEliminarCloud(const String &identificadorStr) {
  if (identificadorStr.isEmpty()) {
    enviarMensajeChat("La cedula no puede estar vacia. Intente de nuevo:", false);
    return;
  }

  int indiceUsuario = buscarIndiceUsuarioPorDocumento(identificadorStr.c_str());

  if (indiceUsuario == -1) {
    enviarMensajeChat("No se encontro usuario con la Cedula '" + identificadorStr + "'. Verifique e intente de nuevo, o escriba 'cancelar'.", false);
    return;
  } else {
    tempIdHuellaHolder = usuarios[indiceUsuario].idHuella;
    strcpy(tempDocumentoHolder, usuarios[indiceUsuario].documento);

    String msg = "Usuario a eliminar:\n";
    msg += "Nombre: " + String(usuarios[indiceUsuario].nombre) + "\n";
    msg += "Cedula: " + String(usuarios[indiceUsuario].documento) + "\n";
    if (strlen(usuarios[indiceUsuario].telefono) > 0) {
      msg += "Telefono: " + String(usuarios[indiceUsuario].telefono) + "\n";
    }
    msg += "ID Huella asociada: " + String(usuarios[indiceUsuario].idHuella) + "\n";
    msg += "¿Esta seguro de eliminar este usuario y su huella? (s/n)";
    enviarMensajeChat(msg, false);
    currentSystemState = DELETE_USER_CONFIRMING;
  }
}
void procesarFechaNacimientoCloud(const String &fechaNac) {
  // Validar formato de fecha
  if (!validarFechaNacimiento(fechaNac)) {
    enviarMensajeChat("Formato de fecha inválido. Use dd/mm/aaaa. Ejemplo: 21/11/2001", false);
    return;
  }

  // Almacenar la fecha de nacimiento
  strncpy(tempUserHolder.fechaNacimiento, fechaNac.c_str(), FECHA_NACIMIENTO_LENGTH - 1);
  tempUserHolder.fechaNacimiento[FECHA_NACIMIENTO_LENGTH - 1] = '\0';
 
  Serial.print(F("CHAT: Fecha de nacimiento recibida: "));
  Serial.println(tempUserHolder.fechaNacimiento);

  // Obtener ID de huella disponible
  tempIdHuellaHolder = obtenerSiguienteIdHuellaDisponible();
  if (tempIdHuellaHolder == 0) {
    resetSessionCloud("Error: No hay IDs de huella disponibles en el sensor o sistema.");
    return;
  }
 
  // Mostrar popup de confirmación con todos los datos
  mostrarPopupConfirmacionUsuario(&tempUserHolder);

  // Ahora sí mostrar mensaje para continuar con huella
  String msg = "Datos de usuario completados.\n";
  msg += "Nombre: " + String(tempUserHolder.nombre) + "\n";
  msg += "Documento: " + String(tempUserHolder.documento) + "\n";
  if (strlen(tempUserHolder.telefono) > 0) {
    msg += "Teléfono: " + String(tempUserHolder.telefono) + "\n";
  }
  msg += "Fecha Nacimiento: " + String(tempUserHolder.fechaNacimiento) + "\n";
  msg += "ID de Huella asignado: " + String(tempIdHuellaHolder) + "\n\n";
  msg += "Para continuar con el registro de huella, escriba 'ok' o 'listo', o coloque su dedo en el sensor.\n";
  msg += "Para cancelar, escriba 'cancelar'.";
 
  enviarMensajeChat(msg, false);
  currentSystemState = ADD_USER_INITIATE_FINGERPRINT;

  // Reactivar interrupción para detección de dedo
  if (!procesandoHuellaActual) {
    int interruptPin = digitalPinToInterrupt(FINGER_DETECT_PIN);
    if (interruptPin != NOT_AN_INTERRUPT) {
      attachInterrupt(interruptPin, ISR_DedoDetectado, FALLING);
      Serial.println(F("PROCESAR_FECHA_NAC: Interrupción de dedo (re)activada."));
    }
  }
}
void confirmarYEliminarUsuarioCloud(const String &confirmacion) {
  String confLower = confirmacion;
  confLower.toLowerCase();

  if (!confLower.equals("s") && !confLower.equals("si")) {
    resetSessionCloud("Eliminacion cancelada por el usuario.");
    return;
  }

  currentSystemState = DELETE_USER_PROCESSING;
  //enviarMensajeChat("Procesando eliminacion...", false);
  ArduinoCloud.update();
  delay(100);

  int indiceUsuario = buscarIndiceUsuarioPorDocumento(tempDocumentoHolder);
  if (indiceUsuario == -1 && tempIdHuellaHolder == 0) {
    resetSessionCloud("Error interno: no se pudo identificar al usuario para eliminar. Operacion cancelada.");
    return;
  }
  if (indiceUsuario == -1 && tempIdHuellaHolder != 0) {
    indiceUsuario = buscarIndiceUsuarioPorHuella(tempIdHuellaHolder);
  }

  bool eliminadoDelSensor = false;
  uint8_t p = FINGERPRINT_NOFINGER;

  if (tempIdHuellaHolder > 0) {
    p = finger.deleteModel(tempIdHuellaHolder);
    if (p == FINGERPRINT_OK) {
      Serial.println(F("Huella eliminada del sensor."));
      eliminadoDelSensor = true;
    } else if (p == FINGERPRINT_BADLOCATION) {
      Serial.println(F("Huella no encontrada en el sensor con ese ID."));
      eliminadoDelSensor = true;
    } else {
      Serial.print(F("Error al eliminar huella del sensor: "));
      Serial.println(p);
    }
  } else if (indiceUsuario != -1 && usuarios[indiceUsuario].idHuella == 0) {
    Serial.println(F("Usuario no tenia ID de huella asignado en la base de datos local. Nada que borrar del sensor."));
    eliminadoDelSensor = true;
  }

  String resultadoMsg = "";
  if (indiceUsuario != -1) {
    resultadoMsg = "ELIMINADO: " + String(usuarios[indiceUsuario].nombre) + "\n Cedula: " + String(usuarios[indiceUsuario].documento) + " ";
    usuarios[indiceUsuario].activo = false;
    //resultadoMsg += "marcado como eliminado del sistema.\n";
  } else {
    resultadoMsg = "No se encontro un usuario en la base de datos con los datos proporcionados, pero ";
  }

  if (tempIdHuellaHolder > 0) {
    if (eliminadoDelSensor) {
      resultadoMsg += "Huella ID " + String(tempIdHuellaHolder);
    } else {
      resultadoMsg += "La huella ID " + String(tempIdHuellaHolder) + " NO se pudo eliminar del sensor (Error: " + String(p) + ").\n";
    }
  }

  if (guardarUsuariosEnUSB()) {
    //resultadoMsg += "Cambios guardados en USB.";
    //usuario eliminado con exito
    reproducirPistaAudio(AUDIO_USER_DELETE);
  } else {
    resultadoMsg += "ERROR CRITICO al guardar cambios en USB. Los cambios podrian no ser permanentes.";
    if (indiceUsuario != -1) usuarios[indiceUsuario].activo = true;
    reproducirPistaAudio(AUDIO_ERROR);
  }
  sendNotification(resultadoMsg);
  resetSessionCloud(resultadoMsg);
}
void mostrarUsuariosCloud() {
  Serial.println(F("CHAT: Mostrando Usuarios."));
  if (!verificarConexionUSB() && totalUsuariosAlmacenados == 0) {
    enviarMensajeChat("Almacenamiento USB no conectado y no hay usuarios en memoria.", true);
    resetSessionCloud();
    return;
  }

  if (totalUsuariosAlmacenados == 0) {
    enviarMensajeChat("No hay usuarios registrados en el sistema.", true);
    resetSessionCloud();
    return;
  }

  String listaMsg = "--- Usuarios Registrados ---\n";
  int contadorActivos = 0;
  for (int i = 0; i < totalUsuariosAlmacenados; i++) {
    if (usuarios[i].activo) {
      contadorActivos++;
      listaMsg += String(contadorActivos) + ". Nombre: " + String(usuarios[i].nombre) + "\n";

      if (strlen(usuarios[i].documento) > 0) {
        listaMsg += "   Cedula: " + String(usuarios[i].documento) + "\n";
      } else {
        listaMsg += "   Cedula: (No registrada)\n";
      }

      if (strlen(usuarios[i].telefono) > 0) {
        listaMsg += "   Telefono: " + String(usuarios[i].telefono) + "\n";
      } else {
        listaMsg += "   Telefono: (No registrado)\n";
      }
      listaMsg += "----------------------------\n";
    }
  }

  if (contadorActivos == 0) {
    listaMsg = "No hay usuarios activos para mostrar.\n";
  }
  listaMsg += "\nTotal usuarios activos: " + String(contadorActivos) + "\n";

  enviarMensajeChat(listaMsg, false);
  resetSessionCloud();
}
void iniciarAjusteVolumenCloud() {
  Serial.println(F("CHAT: Iniciando ajuste de volumen."));
  enviarMensajeChat("Volumen actual: " + String(currentVolume) + ".\nIngrese nuevo nivel (0-" + String(VOLUMEN_MAX) + "):", false);
  currentSystemState = ADJUST_VOLUME_AWAITING_LEVEL;
}
void procesarNuevoVolumenCloud(const String &volStr) {
  int vol = volStr.toInt();
  bool esNumeroValido = false;
  if (volStr.equals("0")) {
    esNumeroValido = true;
    vol = 0;
  } else {
    if (vol > 0 && vol <= VOLUMEN_MAX) {
      esNumeroValido = true;
    }
  }

  if (esNumeroValido) {
    configurarVolumenAudio((uint8_t)vol);
    resetSessionCloud("Volumen ajustado a: " + String(currentVolume));
    if (currentVolume > 0) {
      reproducirPistaAudio(AUDIO_EXITO);
    }
  } else {
    enviarMensajeChat("Valor invalido. Debe ser un numero entre 0 y " + String(VOLUMEN_MAX) + ". Intente de nuevo:", false);
  }
}
bool validarFechaNacimiento(const String &fecha) {
    if (fecha.length() != 10) return false;
    if (fecha[2] != '/' || fecha[5] != '/') return false;
   
    int dia = fecha.substring(0, 2).toInt();
    int mes = fecha.substring(3, 5).toInt();
    int anio = fecha.substring(6).toInt();
   
    // Validación básica
    if (dia < 1 || dia > 31) return false;
    if (mes < 1 || mes > 12) return false;
    if (anio < 1900 || anio > 2100) return false;
   
    return true;
}





// ===== FUNCIONES DEL SISTEMA DE ACCESO (Adaptadas o como estaban) =====

void ISR_DedoDetectado() {
  if (!procesandoHuellaActual && (currentSystemState == IDLE || currentSystemState == MAIN_MENU_AWAITING_CHOICE || currentSystemState == ADD_USER_INITIATE_FINGERPRINT)) {
    dedoDetectadoFlag = true;
  }
}
void inicializarAudio() {
  Serial.println(F("Inicializando reproductor de audio..."));
  MP3_SERIAL.begin(9600);
  delay(500);
  configurarVolumenAudio(VOLUMEN_INICIAL);
  Serial.println(F("Reproductor de audio inicializado."));
}
uint8_t calcularChecksumAudio(const uint8_t *command_bytes, size_t length) {
  uint16_t sum = 0;
  for (size_t i = 0; i < length - 1; i++) {
    sum += command_bytes[i];
  }
  return (uint8_t)sum;
}
bool enviarComandoAudio(const uint8_t *command_bytes, size_t length, uint8_t maxRetries) {
  if (!MP3_SERIAL) {
    Serial.println(F("AUDIO ERROR: Puerto serial MP3_SERIAL no disponible."));
    return false;
  }

  uint8_t retryCount = 0;
  while (retryCount < maxRetries) {
    MP3_SERIAL.write(command_bytes, length);
    delay(50);
    return true;
  }
  Serial.println(F("AUDIO ERROR: Fallo al enviar comando después de reintentos (o no implementado ACK)."));
  return false;
}
void reproducirPistaAudio(uint16_t trackNumber, bool esperarFinalizacion) {
  uint8_t upperByte = (uint8_t)(trackNumber >> 8);
  uint8_t lowerByte = (uint8_t)(trackNumber & 0xFF);
  uint8_t audioCmd[6] = { 0xAA, 0x07, 0x02, upperByte, lowerByte, 0x00 };
  audioCmd[5] = calcularChecksumAudio(audioCmd, 6);

  if (!enviarComandoAudio(audioCmd, sizeof(audioCmd), 1)) {
    Serial.println(F("AUDIO ERROR: No se pudo enviar comando para reproducir pista."));
  }

  if (esperarFinalizacion) {
    delay(2500);
  }
}
void configurarVolumenAudio(uint8_t volume, bool guardarVolumenActual) {
  if (volume > VOLUMEN_MAX) volume = VOLUMEN_MAX;

  if (guardarVolumenActual) {
    currentVolume = volume;
  }

  uint8_t volCmd[5] = { 0xAA, 0x13, 0x01, volume, 0x00 };
  volCmd[4] = calcularChecksumAudio(volCmd, 5);

  if (!enviarComandoAudio(volCmd, sizeof(volCmd), 1)) {
    Serial.println(F("AUDIO ERROR: No se pudo enviar comando para configurar volumen."));
  } else {
    Serial.print(F("AUDIO: Volumen configurado a: "));
    Serial.println(volume);
  }
}
void inicializarLEDsAccesorios() {
  Serial.println(F("Inicializando LEDs Accesorios..."));
  accesorios_led.setColor(RGBLed::RED);
  delay(150);
  accesorios_led.setColor(RGBLed::GREEN);
  delay(150);
  accesorios_led.setColor(RGBLed::BLUE);
  delay(150);
  accesorios_led.off();
  Serial.println(F("LEDs Accesorios inicializados."));
}
void configurarColorLED_Accesorios(uint8_t colorSensor) {
  switch (colorSensor) {
    case FINGERPRINT_LED_RED: accesorios_led.setColor(RGBLed::RED); break;
    case FINGERPRINT_LED_BLUE: accesorios_led.setColor(RGBLed::BLUE); break;
    case FINGERPRINT_LED_PURPLE: accesorios_led.setColor(RGBLed::MAGENTA); break;
    default: accesorios_led.off(); break;
  }
}
void sincronizarLEDsConSensor(uint8_t colorSensor, uint8_t modoSensor, uint8_t velocidadSensor) {
  finger.LEDcontrol(modoSensor, velocidadSensor, colorSensor, 0);
  configurarColorLED_Accesorios(colorSensor);
}




// ===== CORRECCION ERROR COMPILACION INICIO =====
bool inicializarSensorHuellasR503() {
  Serial.println(F("Inicializando sensor de huellas R503..."));
  SENSOR_SERIAL.begin(57600);
  delay(100);  // Esperar un poco a que el puerto serial se estabilice

  for (int intentos = 0; intentos < 3; intentos++) {
    if (finger.verifyPassword()) {
      Serial.println(F("Sensor de huellas R503 conectado y contraseña verificada."));
      finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_OFF, 0);
      finger.getParameters();  // Obtener parámetros del sensor
      Serial.print(F("Capacidad Sensor: "));
      Serial.println(finger.capacity);
      Serial.print(F("Huellas almacenadas: "));
      finger.getTemplateCount();  // Actualiza finger.templateCount
      Serial.println(finger.templateCount);
      return true;  // Sensor inicializado correctamente
    }
    Serial.print(F("Intento "));
    Serial.print(intentos + 1);
    Serial.println(F(" de conexion con sensor fallido."));
    delay(500);
  }
  Serial.println(F("FALLO: No se detecto el sensor de huellas o la contraseña es incorrecta."));
  return false;  // Falló la inicialización del sensor
}



// ===== CORRECCION ERROR COMPILACION FIN =====

uint8_t identificarHuellaRegistrada(uint16_t timeout) {
  unsigned long startTime = millis();
  uint8_t p = FINGERPRINT_NOFINGER;

  while (p != FINGERPRINT_OK && (millis() - startTime < timeout)) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      delay(50);
    } else if (p != FINGERPRINT_OK) {
      Serial.print(F("Error al capturar imagen: "));
      Serial.println(p);
      return 0;
    }
  }
  if (p != FINGERPRINT_OK) {
    return 0;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.print(F("Error al convertir imagen: "));
    Serial.println(p);
    return 0;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print(F("Huella encontrada! ID #"));
    Serial.print(finger.fingerID);
    Serial.print(F(" con confianza "));
    Serial.println(finger.confidence);
    return finger.fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    return 0;
  } else {
    Serial.print(F("Error en la busqueda de huella: "));
    Serial.println(p);
    return 0;
  }
}
void procesarHuellaDetectadaPorWakeUp() {
    sincronizarLEDsConSensor(FINGERPRINT_LED_PURPLE, FINGERPRINT_LED_ON, 0);
    Serial.println("WAKEUP: Verificando huella...");

    uint8_t idHuellaEncontrada = identificarHuellaRegistrada(TIMEOUT_OPERACION_HUELLA / 2);
    String mensajeChatResultado = "";
    String tipoEvento = "ACCESO";  // Valor por defecto

    // Determinar tipo de evento según las banderas
    if (bandera_ingreso_activo) {
        tipoEvento = "INGRESO";
        bandera_ingreso_activo = false;
        ingreso_timer.detach();  // Cancelar timer
        mostrarPopup("Ingreso registrado", LV_SYMBOL_OK, 3);
        sincronizarLEDsConSensor(FINGERPRINT_LED_BLUE, FINGERPRINT_LED_ON, 50);
    } else if (bandera_salida_activo) {
        tipoEvento = "SALIDA";
        bandera_salida_activo = false;
        salida_timer.detach();  // Cancelar timer
        mostrarPopup("Salida registrada", LV_SYMBOL_OK, 3);
        sincronizarLEDsConSensor(FINGERPRINT_LED_RED, FINGERPRINT_LED_ON, 50);
    } else {
        // Wake-up normal (sin botón presionado)
        sincronizarLEDsConSensor(FINGERPRINT_LED_PURPLE, FINGERPRINT_LED_FLASHING, LED_FLASH_TIME);
    }

    if (idHuellaEncontrada > 0) {
        mostrarPopup("Acceso concedido", LV_SYMBOL_OK, 3);
        if(tipoEvento=="INGRESO"){
          reproducirPistaAudio(AUDIO_INGRESO_EXITOSO);
        }else if(tipoEvento=="SALIDA"){
          reproducirPistaAudio(AUDIO_SALIDA_EXITOSO);
        }else{
          reproducirPistaAudio(AUDIO_EXITO);
        }
        int indiceUsuario = buscarIndiceUsuarioPorHuella(idHuellaEncontrada);

        if (indiceUsuario != -1 && usuarios[indiceUsuario].activo) {
            actualizarYAlmacenarHoraFormateadaCloud();

            // Construir mensajes según el tipo de evento
            mensajeChatResultado = tipoEvento + ": " + String(usuarios[indiceUsuario].nombre) +
                                 ", " + String(globalFormattedDate) +
                                 " a las " + String(globalFormattedTime);

            String mensajeNotificacion = tipoEvento + ":\nNombre: " + String(usuarios[indiceUsuario].nombre) +
                          "\nDocumento: " + String(usuarios[indiceUsuario].documento) +
                          "\nFecha: " + String(globalFormattedDate) +
                          "\nHora: " + String(globalFormattedTime);
           
            sendNotification(mensajeNotificacion);

            Serial.print(F("WAKEUP: "));
            Serial.print(tipoEvento);
            Serial.print(F(" registrado para "));
            Serial.print(usuarios[indiceUsuario].nombre);
            Serial.print(F(" (Documento: "));
            Serial.print(usuarios[indiceUsuario].documento);
            Serial.print(F(" (ID Huella: "));
            Serial.print(idHuellaEncontrada);
            Serial.print(F("), Fecha: "));
            Serial.print(globalFormattedDate);
            Serial.print(F(", Hora: "));
            Serial.println(globalFormattedTime);

            intentosFallidosAcceso = 0;
        } else {
            mostrarPopup("Acceso Denegado", LV_SYMBOL_WARNING, 3);
            // Usuario no encontrado o inactivo
            if (indiceUsuario != -1 && !usuarios[indiceUsuario].activo) {
                mensajeChatResultado = "ALERTA: Huella reconocida pero usuario INACTIVO (" + String(usuarios[indiceUsuario].nombre) + ")";
                Serial.print(F("WAKEUP: Usuario INACTIVO detectado: "));
                Serial.println(usuarios[indiceUsuario].nombre);
            } else {
                mensajeChatResultado = "ALERTA: Huella no asociada a usuario registrado";
                Serial.println(F("WAKEUP: Huella no tiene usuario asociado"));

            }

            reproducirPistaAudio(AUDIO_ERROR);
            sincronizarLEDsConSensor(FINGERPRINT_LED_RED, FINGERPRINT_LED_FLASHING, LED_FLASH_TIME);
            intentosFallidosAcceso++;
        }
    } else {
        mostrarPopup("Acceso Denegado", LV_SYMBOL_WARNING, 3);
        // Huella no reconocida
        mensajeChatResultado = tipoEvento + " DENEGADO: Huella no reconocida";
        Serial.println(F("WAKEUP: Huella no reconocida"));
        reproducirPistaAudio(AUDIO_HUELLA_NO);
        sincronizarLEDsConSensor(FINGERPRINT_LED_RED, FINGERPRINT_LED_FLASHING, LED_FLASH_TIME);
        intentosFallidosAcceso++;
    }

    // Manejar múltiples intentos fallidos
    if (intentosFallidosAcceso >= 3) {
        Serial.println(F("WAKEUP: Multiples intentos fallidos. Pausa."));
        delay(RETARDO_ENTRE_INTENTOS * 2);
        intentosFallidosAcceso = 0;
    }

    // Enviar mensaje y limpiar
    enviarMensajeChat(mensajeChatResultado, true);
    delay(TIEMPO_LED_ENCENDIDO);
    apagarTodosLEDs();
}
void sendNotification(String message) {
  notification = message;
}
void onNotificationChange() {
}
bool esperarRetiroDedoDelSensor(unsigned long timeout) {
  Serial.println(F("Esperando retiro de dedo del sensor..."));
  unsigned long startTime = millis();
  while (digitalRead(FINGER_DETECT_PIN) == LOW) {
    if (millis() - startTime > timeout) {
      Serial.println(F("Timeout esperando retiro de dedo."));
      return false;
    }
    delay(50);
  }
  Serial.println(F("Dedo retirado del sensor."));
  delay(200);
  return true;
}
bool inicializarConexionUSB() {
  Serial.println(F("USB: Intentando inicializar y montar..."));
  pinMode(PA_15, OUTPUT);
  digitalWrite(PA_15, HIGH);
  delay(1000);

  unsigned long startTime = millis();
  bool connected = false;
  Serial.print(F("USB: Conectando MSD"));
  while (!connected && (millis() - startTime < TIMEOUT_USB_CONNECTION)) {
    if (msd.connect()) {
      connected = true;
    } else {
      Serial.print(F("."));
      delay(500);
    }
  }

  if (!connected) {
    Serial.println(F("\nUSB: No se detecto el dispositivo MSD (memoria USB)."));
    usbConnected = false;
    return false;
  }
  Serial.println(F("\nUSB: Dispositivo MSD conectado. Intentando montar FATfs..."));

  if (usb.mount(&msd) != 0) {
    Serial.println(F("USB: Error al montar el sistema de archivos FAT. Asegurese que la USB este formateada en FAT32."));
    usbConnected = false;
    return false;
  }

  Serial.println(F("USB: Sistema de archivos montado correctamente."));
  usbConnected = true;
  crearDirectorioSiNoExisteEnUSB("/usb/control_acceso");
  crearDirectorioSiNoExisteEnUSB("/usb/control_acceso/usuarios");

  if (!cargarUsuariosDesdeUSB()) {
    Serial.println(F("USB: No se pudieron cargar usuarios o el archivo de indice no existe (se creara uno nuevo al guardar)."));
  }
  return true;
}
bool verificarConexionUSB() {
  if (usbConnected) {
    DIR *dir = opendir("/usb/");
    if (dir) {
      closedir(dir);
      return true;
    } else {
      Serial.println(F("USB: Fallo al acceder al directorio raiz. Conexion perdida."));
      usbConnected = false;
      return false;
    }
  }
  Serial.println(F("USB: No estaba conectado. Intentando inicializar ahora..."));
  return inicializarConexionUSB();
}
bool cargarUsuariosDesdeUSB() {
  if (!usbConnected) {
    Serial.println(F("USB Cargar: USB no conectado."));
    return false;
  }
  Serial.println(F("USB: Cargando usuarios desde index.dat..."));
  FILE *indexFile = fopen("/usb/control_acceso/index.dat", "rb");

  if (!indexFile) {
    Serial.println(F("USB: index.dat no encontrado. Se asumira que no hay usuarios guardados."));
    totalUsuariosAlmacenados = 0;
    return true;
  }

  int usuariosEnArchivo = 0;
  if (fread(&usuariosEnArchivo, sizeof(int), 1, indexFile) != 1) {
    Serial.println(F("USB: Error al leer la cantidad de usuarios desde index.dat."));
    fclose(indexFile);
    totalUsuariosAlmacenados = 0;
    return false;
  }

  Serial.print(F("USB: Usuarios reportados en archivo: "));
  Serial.println(usuariosEnArchivo);
  totalUsuariosAlmacenados = 0;

  for (int i = 0; i < usuariosEnArchivo; ++i) {
    DatosUsuario tempUsr;
    if (fread(&tempUsr, sizeof(DatosUsuario), 1, indexFile) != 1) {
      Serial.print(F("USB: Error al leer datos del usuario #"));
      Serial.print(i);
      Serial.println(F(" desde index.dat."));
      break;
    }
    if (tempUsr.activo) {
      if (totalUsuariosAlmacenados < MAX_USUARIOS_EN_MEMORIA) {
        usuarios[totalUsuariosAlmacenados++] = tempUsr;
      } else {
        Serial.println(F("USB: Limite de usuarios en RAM (MAX_USUARIOS_EN_MEMORIA) alcanzado al cargar desde USB."));
        break;
      }
    }
  }
  fclose(indexFile);
  Serial.print(F("USB: "));
  Serial.print(totalUsuariosAlmacenados);
  Serial.println(F(" usuarios activos cargados en memoria."));
  return true;
}
bool guardarUsuariosEnUSB() {
  if (!verificarConexionUSB()) {
    Serial.println(F("USB Guardar: USB no conectado o no montado. No se puede guardar."));
    return false;
  }

  Serial.println(F("USB: Guardando usuarios en index.dat..."));
  FILE *indexFile = fopen("/usb/control_acceso/index.dat", "wb");
  if (!indexFile) {
    Serial.println(F("USB: Error critico al crear/abrir index.dat para escritura."));
    return false;
  }

  int activosParaGuardar = 0;
  for (int i = 0; i < totalUsuariosAlmacenados; ++i) {
    if (usuarios[i].activo) {
      activosParaGuardar++;
    }
  }

  if (fwrite(&activosParaGuardar, sizeof(int), 1, indexFile) != 1) {
    Serial.println(F("USB: Error al escribir la cantidad de usuarios en index.dat."));
    fclose(indexFile);
    return false;
  }

  for (int i = 0; i < totalUsuariosAlmacenados; ++i) {
    if (usuarios[i].activo) {
      if (fwrite(&usuarios[i], sizeof(DatosUsuario), 1, indexFile) != 1) {
        Serial.print(F("USB: Error al escribir datos del usuario '"));
        Serial.print(usuarios[i].nombre);
        Serial.println(F("' en index.dat."));
        fclose(indexFile);
        return false;
      }
    }
  }

  if (fflush(indexFile) != 0) {
    Serial.println(F("USB: Error al hacer flush de datos a index.dat."));
  }

  fclose(indexFile);
  Serial.print(F("USB: "));
  Serial.print(activosParaGuardar);
  Serial.println(F(" usuarios activos guardados en index.dat."));
  return true;
}
int buscarIndiceUsuarioPorHuella(uint8_t idHuellaBuscada) {
  if (idHuellaBuscada == 0) return -1;
  for (int i = 0; i < totalUsuariosAlmacenados; ++i) {
    if (usuarios[i].activo && usuarios[i].idHuella == idHuellaBuscada) {
      return i;
    }
  }
  return -1;
}
int buscarIndiceUsuarioPorDocumento(const char *documentoBuscado) {
  if (documentoBuscado == nullptr || strlen(documentoBuscado) == 0) return -1;
  for (int i = 0; i < totalUsuariosAlmacenados; ++i) {
    if (usuarios[i].activo && strcmp(usuarios[i].documento, documentoBuscado) == 0) {
      return i;
    }
  }
  return -1;
}




// ===== CORRECCION ERROR COMPILACION INICIO =====
uint8_t obtenerSiguienteIdHuellaDisponible() {
  if (!sensorHuellasInicializadoOK) {  // Usar la nueva variable global
    Serial.println(F("OBTENER_ID: Sensor de huellas no disponible o no inicializado."));
    return 0;
  }
  // ===== CORRECCION ERROR COMPILACION FIN =====

  for (uint16_t idPropuesto = 1; idPropuesto <= finger.capacity; ++idPropuesto) {
    if (idPropuesto > 255) {
      Serial.println(F("OBTENER_ID: Se supero el limite de 255 para IDs de huella (uint8_t)."));
      return 0;
    }

    bool usadoEnSistemaLocal = false;
    for (int i = 0; i < totalUsuariosAlmacenados; ++i) {
      if (usuarios[i].activo && usuarios[i].idHuella == (uint8_t)idPropuesto) {
        usadoEnSistemaLocal = true;
        break;
      }
    }

    if (!usadoEnSistemaLocal) {
      return (uint8_t)idPropuesto;
    }
  }
  Serial.println(F("OBTENER_ID: No hay IDs de huella disponibles (segun capacidad del sensor y usuarios locales)."));
  return 0;
}
void crearDirectorioSiNoExisteEnUSB(const char *dirPath) {
  if (!usbConnected) return;
  DIR *dir = opendir(dirPath);
  if (dir) {
    closedir(dir);
  } else {
    if (mkdir(dirPath, 0777) == 0) {
      Serial.print(F("USB: Directorio '"));
      Serial.print(dirPath);
      Serial.println(F("' creado."));
    } else {
      Serial.print(F("USB: Error al crear directorio '"));
      Serial.print(dirPath);
      Serial.println(F("'."));
    }
  }
}
bool registrarNuevaHuellaParaUsuario(uint8_t idHuellaDestino) {
  int p = -1;
  apagarTodosLEDs();
  Serial.println(F("------------------------------------"));
  Serial.print(F("REGISTRO DE HUELLA PARA ID: "));
  Serial.println(idHuellaDestino);
  Serial.println(F("------------------------------------"));

  Serial.println(F("PASO 1/2: Coloque el dedo en el sensor..."));
  sincronizarLEDsConSensor(FINGERPRINT_LED_BLUE, FINGERPRINT_LED_ON, 0);
  //reproducirPistaAudio(AUDIO_COLOQUE_DEDO, false);

  unsigned long startTime = millis();
  p = -1;
  while (p != FINGERPRINT_OK && (millis() - startTime < TIMEOUT_OPERACION_HUELLA)) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(F("."));
      delay(100);
    } else if (p != FINGERPRINT_OK) {
      Serial.print(F("\nError capturando imagen 1: "));
      Serial.println(p);
      apagarTodosLEDs();
      reproducirPistaAudio(AUDIO_ERROR);
      return false;
    }
  }
  if (p != FINGERPRINT_OK) {
    Serial.println(F("\nTimeout/Error final capturando imagen 1."));
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("\nImagen 1 capturada. Procesando..."));

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.print(F("Error convirtiendo imagen 1 a plantilla: "));
    Serial.println(p);
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("Plantilla 1 creada. Retire el dedo."));
  reproducirPistaAudio(AUDIO_RETIRE_DEDO, false);
  apagarTodosLEDs();

  if (!esperarRetiroDedoDelSensor(TIMEOUT_OPERACION_HUELLA / 2)) {
    Serial.println(F("Timeout esperando que se retire el dedo (Paso 1)."));
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("Dedo retirado."));
  delay(500);

  Serial.println(F("PASO 2/2: Coloque el MISMO dedo OTRA VEZ en el sensor..."));
  sincronizarLEDsConSensor(FINGERPRINT_LED_BLUE, FINGERPRINT_LED_ON, 0);
  reproducirPistaAudio(AUDIO_COLOQUE_DEDO2, false);

  startTime = millis();
  p = -1;
  while (p != FINGERPRINT_OK && (millis() - startTime < TIMEOUT_OPERACION_HUELLA)) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(F("."));
      delay(100);
    } else if (p != FINGERPRINT_OK) {
      Serial.print(F("\nError capturando imagen 2: "));
      Serial.println(p);
      apagarTodosLEDs();
      reproducirPistaAudio(AUDIO_ERROR);
      return false;
    }
  }
  if (p != FINGERPRINT_OK) {
    Serial.println(F("\nTimeout/Error final capturando imagen 2."));
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("\nImagen 2 capturada. Procesando..."));

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.print(F("Error convirtiendo imagen 2 a plantilla: "));
    Serial.println(p);
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("Plantilla 2 creada."));

  Serial.print(F("Creando modelo para ID "));
  Serial.println(idHuellaDestino);
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    if (p == FINGERPRINT_ENROLLMISMATCH) {
      Serial.println(F("Error: Las huellas no coinciden!"));
    } else {
      Serial.print(F("Error al crear el modelo de huella: "));
      Serial.println(p);
    }
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }
  Serial.println(F("Modelo de huella creado."));

  Serial.print(F("Almacenando modelo en el sensor con ID "));
  Serial.println(idHuellaDestino);
  p = finger.storeModel(idHuellaDestino);
  if (p != FINGERPRINT_OK) {
    Serial.print(F("Error al almacenar el modelo en el sensor (ID podria estar ocupado o fuera de rango): "));
    Serial.println(p);
    apagarTodosLEDs();
    reproducirPistaAudio(AUDIO_ERROR);
    return false;
  }

  Serial.println(F(">>> HUELLA REGISTRADA EXITOSAMENTE EN SENSOR! <<<"));
  sincronizarLEDsConSensor(FINGERPRINT_LED_PURPLE, FINGERPRINT_LED_FLASHING, 200);
  reproducirPistaAudio(AUDIO_EXITO, true);
  delay(1000);
  apagarTodosLEDs();
  Serial.println(F("------------------------------------"));
  return true;
}
void manejarTimeoutOperacionDispositivo() {
}



//----------CAMBIOS PARA ACCESO PERSONALIZADO-----------------------------
bool validarFormatoFecha(const String &fecha) {
    if (fecha.length() != 10) return false;
    if (fecha[2] != '/' || fecha[5] != '/') return false;
   
    int dia = fecha.substring(0, 2).toInt();
    int mes = fecha.substring(3, 5).toInt();
    int anio = fecha.substring(6).toInt();
   
    // Validación básica
    if (dia < 1 || dia > 31) return false;
    if (mes < 1 || mes > 12) return false;
    if (anio < 2023 || anio > 2100) return false;
   
    return true;
}
int compararFechas(const char* fecha1, const char* fecha2) {
    int dia1 = atoi(fecha1);
    int mes1 = atoi(fecha1 + 3);
    int anio1 = atoi(fecha1 + 6);
   
    int dia2 = atoi(fecha2);
    int mes2 = atoi(fecha2 + 3);
    int anio2 = atoi(fecha2 + 6);
   
    if (anio1 != anio2) return anio1 - anio2;
    if (mes1 != mes2) return mes1 - mes2;
    return dia1 - dia2;
}
String obtenerFechaActualFormateada() {
    time_t now = ArduinoCloud.getLocalTime();
    if (now == 0) return "01/01/2023"; // Fecha por defecto
   
    struct tm *timeinfo = localtime(&now);
    char buffer[11];
    sprintf(buffer, "%02d/%02d/%04d",
            timeinfo->tm_mday,
            timeinfo->tm_mon + 1,
            timeinfo->tm_year + 1900);
    return String(buffer);
}
String obtenerFechaFinAcceso(const char* clave) {
    uint16_t claveIngresada = atoi(clave);
    for(int i = 0; i < totalAccesosPersonalizados; i++) {
        if(accesosPersonalizados[i].clave == claveIngresada) {
            return String(accesosPersonalizados[i].fechaFin);
        }
    }
    return "Fecha no encontrada";
}
bool claveExiste(uint16_t clave) {
    // Verificar en accesos personalizados
    for (int i = 0; i < totalAccesosPersonalizados; i++) {
        if (accesosPersonalizados[i].clave == clave) {
            return true;
        }
    }
    // Verificar si coincide con la clave por defecto
    return (clave == atoi(default_password));
}
uint16_t generarClaveUnica() {
    uint16_t nuevaClave;
    do {
        nuevaClave = random(1000, 9999);
    } while (claveExiste(nuevaClave));
    return nuevaClave;
}
//----------CAMBIOS PARA ACCESO PERSONALIZADO-----------------------------USB
bool cargarAccesosPersonalizadosDesdeUSB() {
    if (!verificarConexionUSB()) return false;

    FILE *file = fopen("/usb/control_acceso/accesos.dat", "rb");
    if (!file) return true; // No existe archivo aún, no es error

    // Leer cantidad de accesos
    uint16_t count;
    if (fread(&count, sizeof(uint16_t), 1, file) != 1) {
        fclose(file);
        return false;
    }

    // Leer accesos
    totalAccesosPersonalizados = 0;
    for (int i = 0; i < min(count, MAX_ACCESOS_PERSONALIZADOS); i++) {
        if (fread(&accesosPersonalizados[i], sizeof(AccesoPersonalizado), 1, file) == 1) {
            totalAccesosPersonalizados++;
        }
    }

    fclose(file);
    return true;
}
bool guardarAccesosPersonalizadosEnUSB() {
    if (!verificarConexionUSB()) return false;

    FILE *file = fopen("/usb/control_acceso/accesos.dat", "wb");
    if (!file) return false;

    // Escribir cantidad de accesos
    if (fwrite(&totalAccesosPersonalizados, sizeof(uint16_t), 1, file) != 1) {
        fclose(file);
        return false;
    }

    // Escribir accesos
    for (int i = 0; i < totalAccesosPersonalizados; i++) {
        if (fwrite(&accesosPersonalizados[i], sizeof(AccesoPersonalizado), 1, file) != 1) {
            fclose(file);
            return false;
        }
    }

    fclose(file);
    return true;
}

//----------CAMBIOS PARA ACCESO PERSONALIZADO-----------------------------FLUJO
void iniciarFlujoAccesoPersonalizado() {
    memset(&tempAccess, 0, sizeof(AccesoPersonalizado));
    enviarMensajeChat("Creando acceso personalizado\nIngrese nombre (max 30 caracteres):", false);
    currentSystemState = AWAITING_ACCESS_NAME;
}
void procesarNombreAccesoPersonalizado(const String &nombre) {
    if (nombre.isEmpty() || nombre.length() >= NOMBRE_MAX_LENGTH) {
        enviarMensajeChat("Nombre inválido. Intente de nuevo (max 30 chars):", false);
        return;
    }
   
    strncpy(tempAccess.nombre, nombre.c_str(), NOMBRE_MAX_LENGTH - 1);
    tempAccess.nombre[NOMBRE_MAX_LENGTH - 1] = '\0';
   
    enviarMensajeChat("Ingrese fecha de inicio (DD/MM/YYYY):", false);
    currentSystemState = AWAITING_ACCESS_START_DATE;
}
void procesarFechaInicioAcceso(const String &fecha) {
    if (!validarFormatoFecha(fecha)) {
        enviarMensajeChat("Formato de fecha inválido. Use DD/MM/YYYY:", false);
        return;
    }
   
    strncpy(tempAccess.fechaInicio, fecha.c_str(), 10);
    tempAccess.fechaInicio[10] = '\0';
   
    enviarMensajeChat("Ingrese fecha de fin (DD/MM/YYYY):", false);
    currentSystemState = AWAITING_ACCESS_END_DATE;
}
void procesarFechaFinAcceso(const String &fecha) {
    if (!validarFormatoFecha(fecha)) {
        enviarMensajeChat("Formato de fecha inválido. Use DD/MM/YYYY:", false);
        return;
    }
   
    // Validar que fecha fin sea posterior a fecha inicio
    if (compararFechas(tempAccess.fechaInicio, fecha.c_str()) > 0) {
        enviarMensajeChat("Fecha fin debe ser posterior a fecha inicio. Intente nuevamente:", false);
        return;
    }
   
    // Copiar la fecha de fin
    strncpy(tempAccess.fechaFin, fecha.c_str(), 10);
    tempAccess.fechaFin[10] = '\0';
   
    // Generar clave única
    tempAccess.clave = generarClaveUnica();
   
    // Guardar el acceso (si hay espacio)
    if (totalAccesosPersonalizados < MAX_ACCESOS_PERSONALIZADOS) {
        memcpy(&accesosPersonalizados[totalAccesosPersonalizados], &tempAccess, sizeof(AccesoPersonalizado));
        totalAccesosPersonalizados++;
       
        if (guardarAccesosPersonalizadosEnUSB()) {
            String msg = "Acceso creado exitosamente!\n";
            msg += "Nombre: " + String(tempAccess.nombre) + "\n";
            msg += "Clave: " + String(tempAccess.clave) + "\n";
            msg += "Válido desde: " + String(tempAccess.fechaInicio) + " hasta " + String(tempAccess.fechaFin);
            resetSessionCloud(msg);
        } else {
            resetSessionCloud("Error al guardar acceso. Intente nuevamente.");
        }
    } else {
        resetSessionCloud("Límite de accesos personalizados alcanzado. No se pudo guardar.");
    }
}
bool verificarAccesoPersonalizado(const char* clave, String &nombreUsuario) {
    if (clave == nullptr) {
        Serial.println("[ERROR] Clave es NULL en verificarAccesoPersonalizado");
        return false;
    }

    String fechaActual = obtenerFechaActualFormateada();
    uint16_t claveIngresada = atoi(clave);
   
    Serial.print("[DEBUG] Verificando acceso personalizado. Clave ingresada: ");
    Serial.print(claveIngresada);
    Serial.print(", Fecha actual: ");
    Serial.println(fechaActual);

    for(int i = 0; i < totalAccesosPersonalizados; i++) {
        Serial.print("[DEBUG] Comparando con acceso #");
        Serial.print(i);
        Serial.print(" - Clave: ");
        Serial.print(accesosPersonalizados[i].clave);
        Serial.print(", Nombre: ");
        Serial.print(accesosPersonalizados[i].nombre);
        Serial.print(", Valido desde ");
        Serial.print(accesosPersonalizados[i].fechaInicio);
        Serial.print(" hasta ");
        Serial.println(accesosPersonalizados[i].fechaFin);

        if(accesosPersonalizados[i].clave == claveIngresada) {
            int cmpInicio = compararFechas(fechaActual.c_str(), accesosPersonalizados[i].fechaInicio);
            int cmpFin = compararFechas(fechaActual.c_str(), accesosPersonalizados[i].fechaFin);
           
            Serial.print("[DEBUG] Resultado comparacion fechas - Inicio: ");
            Serial.print(cmpInicio);
            Serial.print(", Fin: ");
            Serial.println(cmpFin);

            if(cmpInicio >= 0 && cmpFin <= 0) {
                nombreUsuario = String(accesosPersonalizados[i].nombre);
                Serial.println("[DEBUG] Acceso personalizado VALIDO");
                return true;
            } else {
                Serial.println("[DEBUG] Acceso personalizado INVALIDO (fuera de rango de fechas)");
            }
        }
    }
    Serial.println("[DEBUG] Ningun acceso personalizado coincide");
    return false;
}
