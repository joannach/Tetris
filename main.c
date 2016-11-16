/*******************************************************************************
 * używane biblioteki
 */
//#include <stdio.h>
//#include <stdlib.h>
#include "diag/Trace.h"
#include "Timer.h"
#include "BlinkLed.h"
#include "LCD.h"
#include "stm32f4xx_hal_tim.h"



// Keep the LED on for 2/3 of a second.
#define BLINK_ON_TICKS  (TIMER_FREQUENCY_HZ * 2 / 3)
#define BLINK_OFF_TICKS (TIMER_FREQUENCY_HZ - BLINK_ON_TICKS)

/*
 * Komendy preprocesora
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Unused-parameter"
#pragma GCC diagnostic ignored "-Missing-declarations"
#pragma GCC diagnostic ignored "-Return-type"


/*******************************************************************************
 * definicje używane w programie
 */
#define BLINK_GPIOx(_N)                 ((GPIO_TypeDef *)(GPIOA_BASE + (GPIOB_BASE-GPIOA_BASE)*(_N)))
#define BLINK_PIN_MASK(_N)              (1 << (_N))
#define BLINK_RCC_MASKx(_N)             (RCC_AHB1ENR_GPIOAEN << (_N))

/* Rozmiar wyświetlacza */
#define LCD_SZEROKOSC 84
#define LCD_WYSOKOSC 48

/* rodzaje używanych klockow */
#define KWADRAT 0
#define PROSTOKAT 1

/* możliwe pozycje prostokątnego klocka */
#define PROSTOKAT_PION 0
#define PROSTOKAT_POZIOM 1


/*******************************************************************************
 * Deklaracje Funkcji
 */
void init_PA1 (void);
void init_PA2 (void);
void init_PA3 (void);
void init_PA4 (void);
void EXTI1_IRQHandler(void);
void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void rysuj_ekran (char* tablica_pikseli);
void klocki_na_piksele(void);
void LCD_INIT(void);
void dodaj_klocek(void);
int sprawdz_klocek(void);
void koniec_gry(void);
void usun_linie(int numer_linii);
void szukaj_linii(void);
void gra_tick(void);

/*******************************************************************************
 * Zmienne lokalne
 */
/* Tablica do rysowania. Obrazy sa rysowane paskami po 8 pikseli */
static char piksele[6][LCD_SZEROKOSC]; // LCD_WYSOKOSC/8 = 48/8 = 6

/* Tablica do przechowywania pozycji klocków XY */
static char klocki[LCD_WYSOKOSC][LCD_SZEROKOSC]; /* cała plansza */
static char klocek[LCD_WYSOKOSC][LCD_SZEROKOSC]; /* bieżący klocek */
static char plansza[LCD_WYSOKOSC][LCD_SZEROKOSC]; /* klocki już ustawione */


/*******************************************************************************
 * zmienne lokalne
 */
static char jest_klocek;
static int najnizsza_linia;
static int lewa_linia;
static int prawa_linia;
static int ostateczny_wynik;
static char losowy_licznik;
static char biezacy_klocek;
static char pozycja_klocka;
static char ruchy;
static char start_gry; /* określa stan gry: 0-stop gry; 1-gra w toku */


/*******************************************************************************
 * Funckja służąca do zamiany tablicy z klockami na tablicę z pikselami.
 */
void klocki_na_piksele(void)
{
   int i;
   int j;

   /* dodaj ramkę na planszy */
   for (j=0; j< LCD_SZEROKOSC-1; j++)
   {
      klocki[0][j] = 1;
      klocki[LCD_WYSOKOSC-1][j] = 1;
   }

   /*
    * Każdy element tablicy 'piksele' to w sumie 8 pikseli na wyświetlaczu. W
    * takim razie każdy z elementów tablicy 'piksele'powstaje przy użyciu 8
    * elementów z tablicy 'klocki'.
    */
   for (i=0; i< LCD_WYSOKOSC/8; i++)
   {
      for (j=0; j< LCD_SZEROKOSC; j++)
      {
         piksele[i][j] = (klocki[i*8][j]) |
                         (klocki[i*8+1][j] << 1) |
                         (klocki[i*8+2][j] << 2) |
                         (klocki[i*8+3][j] << 3) |
                         (klocki[i*8+4][j] << 4) |
                         (klocki[i*8+5][j] << 5) |
                         (klocki[i*8+6][j] << 6) |
                         (klocki[i*8+7][j] << 7);
      }
   }
}


/*******************************************************************************
 * Rysowanie klocków na wyświetlaczu
 */
void rysuj_ekran (char* tablica_pikseli)
{
   int i;

   /* zamień tablicę z pozycjami klocków na tablicę przechowywująca konkretne
    * piksele */
   klocki_na_piksele();

   /* ustaw kursor na początku wyświetlacza i wyrusuj tablice pikseli */
   LCD5110_set_XY(0,0);
   for(i = 0; i < 6*84; i++)
   {
      LCD5110_LCD_write_byte(tablica_pikseli[i]);
   }
}

/*******************************************************************************
 * Inicjalizacja wyświetlacza:
 * - inicjalizacja (wyzerowanie) zmiennych systemowych
 * - uruchominie obsługi LCD
 * - wyświetlenie autorów
 * - wyczyszczenie ekranu
 * - uruchomienie gry
 */
void LCD_INIT(void)
{
   int i;
   int j;

   /* wyzerowanie (zainicjalizowanie) zmiennych */
   losowy_licznik = 0;
   start_gry = 0;
   jest_klocek = 0;
   ostateczny_wynik = 0;

   /*
    * uruchomienie sterowników LCD
    */
   LCD5110_init();

   /*
    * Wypisanie autorów
    */
   LCD5110_set_XY(0,0);
   LCD5110_write_string("TETRIS!!");
   LCD5110_set_XY(0,1);
   LCD5110_write_string("Wykonanie: ");
   LCD5110_set_XY(0,2);
   LCD5110_write_string("J. Kawczynska");
   LCD5110_set_XY(0,3);
   LCD5110_write_string("J. Chrominska");

   /*
    * Po wypisaniu autorów gry, trwa oczekiwanie na wduszenie przycisku 4.
    * Guzik ten powoduje przerwanie obsługiwane przez funkcję EXTI4_IRQHandler().
    */
   while(!start_gry);

   /*
    * Wyczyszczenie (inicjalizacja) wsyzstkich tablic przed rozpoczeciem gry
    */
   for(i = 0; i < LCD_WYSOKOSC; i++)
   {
      for(j = 0; j < LCD_SZEROKOSC; j++)
      {
               klocek[i][j] = 0;
               klocki[i][j] = 0;
               plansza[i][j] = 0;
      }
   }


   /*
    * Wypełnienie ekranu na czarno
    */
   LCD5110_set_XY(0,0);
   for(i = 0; i < LCD_WYSOKOSC; i++)
   {
      for(j = 0; j < LCD_SZEROKOSC; j++)
      {
               klocki[i][j] = 1;
      }
   }

   rysuj_ekran((char*)piksele);
   i = 30000000;
   while(i--);

   /*
    * Wyczyszczenie ekranu
    */
   LCD5110_set_XY(0,0);
   for(i = 0; i < LCD_WYSOKOSC; i++)
   {
      for(j = 0; j < LCD_SZEROKOSC; j++)
      {
               klocki[i][j] = 0;
      }
   }
   rysuj_ekran((char*)piksele);
   i = 30000000;
   while(i--);

   /* uruchomienie gry */
   add_timer_task(TIMER_FREQUENCY_HZ);
}

/*******************************************************************************
 * Dodanie nowego klocka na planszy:
 * - rodzaj klocka jest losowany na podstawie zmiennej 'losowy_licznik'
 * - po wylosowaniu klocka jest on dodawany na planszy oraz ustawiane są wartości
 *   zmiennych 'lewa_linia', 'prawa_linia', 'najnizsza_linia', 'biezacy_klocek'
 *   oraz 'pozycja_klocka'
 * - zmienne 'lewa_linia', 'prawa_linia', 'najnizsza_linia' są uzywane do określania
 *   pozycji klocka na planszy
 * - zmienne 'biezacy_klocek' oraz 'pozycja_klocka' są używane do określenia
 *   rodzaju klocka oraz jego orientacji (pion/poziom) podczas obracania (guzik trzeci)
 */
void dodaj_klocek(void)
{
   int i;
   int j;

   /* kolcek jest 'losowany' na podstawie 'losowy_licznik' */
   losowy_licznik = losowy_licznik%2;
   ruchy = 0;

   /* wyrysuj klocek na podstawie losowania */
   switch(losowy_licznik)
   {

   /* prostokąt */
   case 0:

      /* prostokąt ma rozmiary 2x16 */
      for(i=0; i<2; i++)
      {
         for(j=0; j<16; j++)
         {
            klocek[LCD_WYSOKOSC/2 - 2 + i][j] = 1;
         }
      }

      /* dodatkowe ustawienie zmiennych */
      najnizsza_linia = 16;
      lewa_linia = LCD_WYSOKOSC/2;
      prawa_linia = LCD_WYSOKOSC/2 + i;
      biezacy_klocek = PROSTOKAT;
      pozycja_klocka = PROSTOKAT_PION;
      break;

   /* kwadrat */
   case 1:
   default:

      /* kwadrat ma rozmiary 8x8 */
      for(i=0; i<8; i++)
      {
         for(j=0; j<8; j++)
         {
            klocek[LCD_WYSOKOSC/2 - 2 + i][j] = 1;
         }
      }

      /* dodatkowe ustawienie zmiennych */
      najnizsza_linia = 8;
      lewa_linia = LCD_WYSOKOSC/2 - 2-4;
      prawa_linia = LCD_WYSOKOSC/2 - 2 + i-4;
      biezacy_klocek = KWADRAT;
      break;
   }
}

/*******************************************************************************
 * Sprawdzenie pozycji klocka.
 * Przyjmujemy, że klocek został postawiony gdy dojechał na sam dół albo gdy
 * został postawiony na innym klocku.
 *
 * Zwracane wartosci:
 * 0 - gra może być kontynuowana
 * 1 - koniec gry
 */
int sprawdz_klocek(void)
{
   int i;
   int j;
   int najwyzsza_linia;
   int klocek_dotarl;
   int wynik = 0;

   klocek_dotarl = 0;


   /* sprawdź czy klocek dojechał na dół */
      if(najnizsza_linia == LCD_SZEROKOSC-1)
   {
      klocek_dotarl = 1;
   }
   /* lub czy nie stanął na innym klocku */
   else
   {
      for(i = 1; i<LCD_WYSOKOSC; i++)
      {
         if((klocek[i][najnizsza_linia-1] == 1) &&
            (plansza[i][najnizsza_linia] == 1))
         {
            klocek_dotarl = 1;
            break;
         }
      }
   }

   /* sprawdz czy potrzebny bedzie nowy klocek lub czy gra nie dobiegła końca */
   if(klocek_dotarl == 1)
   {
      /* zakładamy że gra skończy się, gdy ostatni klocek dotknie górnej krawędzi
       * wyświetlacza */

      /* sprawdź czy gra dobiegła końca */
      if((najnizsza_linia <= 8) || (ruchy < 2))
      {
         /* klocek dotarł do górnej krawędzi planszy - zasygnalizuj koniec gry */
         wynik = 1;
      }
      /* trzeba będze dodac nowy klocek:
       * - skopiowanie klocka z tablicy 'klocek' do tablicy 'plansza'
       * - zasygnalizowanie systemowi, że trzeba dodac nowy klocek (przy pomocy
       *   zmiennej 'jest_klocek') */
      else
      {
         /* żeby przyspieszyc działanie gry, kopiowane są tylko linie na których
          * może aktualnie znajdować się klocek */
         najwyzsza_linia = najnizsza_linia - 16;
         if(najwyzsza_linia<0) najwyzsza_linia = 0;

         /* dodaj klocek do planszy */
         for(i = 0; i < LCD_WYSOKOSC; i++)
         {
            for(j = najwyzsza_linia; j < LCD_SZEROKOSC; j++)
            {
                     plansza[i][j] |= klocek[i][j];
                     klocek[i][j] = 0;
            }
         }

         /* daj znać, ze potrzebny jest nowy klocek */
         jest_klocek = 0;
      }

   }

   /* daj znac czy gra toczy się dalej czy też dobiegła końcowi */
   return wynik;
}

/*******************************************************************************
 * Zakończenie gry i wyświetlenie wyniku.
 * Po wyświetleniu wyniku gra może zostać uruchomiona ponownie przy pomocy
 * przycisku czwartego.
 */
void koniec_gry(void)
{
   int i, j;
   int wynik[4];

   /* zatrzymaj grę */
   start_gry = 0;

   /* wyłącz timer */
//   rysuj_ekran(&piksele[0][0]);

   /* wyczyść ekran */
   LCD5110_set_XY(0,0);
   for(i = 0; i < LCD_WYSOKOSC; i++)
   {
      for(j = 0; j < LCD_SZEROKOSC; j++)
      {
               klocki[i][j] = 0;
      }
   }
   rysuj_ekran((char*)piksele);

   /* oblicz i wyświetl wynik */
   LCD5110_set_XY(0,1);
   LCD5110_write_string("KONIEC GRY");
   LCD5110_set_XY(0,2);
   LCD5110_write_string("Wynik: ");
   LCD5110_set_XY(0,3);

   /* zakładamy, ze wynik bedzie nie wiekszy niż 999
    * Zatem wyświetlany wynik bedzie się składał z liczby setek, dziesiątek oraz
    * jedności */
   /* Do każdej obliczonej cyfry dodawana jest wartość 0x30.
    * Wynika to z faktu, że obliczony wynik to wartość liczbowa, a żeby go
    * wydrukować, trzeba go podać do sterownika w formacie ASCII. Cyfry w
    * tablicy ASCII zaczynają się właśnie od wartości 0x30. */

   wynik[0] = ostateczny_wynik/100 + 0x30; /* obliczenie setek */
   wynik[1] = (ostateczny_wynik%100)/10 + 0x30; /* obliczenie dziesiątek */
   wynik[2] = (ostateczny_wynik%10) + 0x30; /* obliczenie jedności */
   wynik[3] = 0x0;
   LCD5110_write_string((char *)wynik);

   /* w tym momencie nastąpił ostateczny koniec działania gry, jednak przy
    * pomocy guzika czwartego można uruchomić grę ponownie */
   while(!start_gry)
   {
      /* czekaj na wciśnięcie guzka czwartego */
   }

   /* ponowny start gry */
   LCD_INIT();
}

/*
 * usuwanie zapełnionej linii z planszy
 */
void usun_linie(int numer_linii)
{
   int j;
   int i;

   /* usuń linię */
   for(j = 0;j<LCD_WYSOKOSC; j++)
   {
      klocki[j][numer_linii] = 0;
   }

   /* przesuń wszystkie pozostałe linie w dół */
   for(i = numer_linii ; i>0; i--)
   {
      for(j = 0;j<LCD_WYSOKOSC; j++)
      {
         klocki[j][i] = klocki[j][i-1];
         plansza[j][i] = klocki[j][i-1];
      }
   }

   ostateczny_wynik++;
   rysuj_ekran(&piksele[0][0]);
}

/*******************************************************************************
 * Czukanie całych linii w okolicach w której był ostatnio postawiony klocek.
 * Znalezienie kompletnej linii powoduje usunięcie jej z planszy oraz zwiększenie
 * wyniku.
 */
void szukaj_linii(void)
{
   int i, j;
   int licznik;

   /* label używany do wielokrotnego przeszukiwania planszy */
   jeszcze_raz:

   /* szukaj na każdej linii wyświetlacza po kolei */
   for(i = 0; i<LCD_SZEROKOSC; i++)
   {
      licznik = 0;

      /* dla każdej linii zliczamy liczbę czarnych (zapalonych) pikseli */
      for(j = 0;j<LCD_WYSOKOSC; j++)
      {
         if(klocki[j][i] == 1)
         {
            licznik++;
         }
      }

      /* jeżeli każdy piksel na tej linii był zapalony, to usun linie i zacznij
       * szukanie od nowa */
      if(licznik == LCD_WYSOKOSC)
      {
         usun_linie(i);/* usuniecie linii */
         goto jeszcze_raz;/* szukanie od nowa */
         break;
      }
   }
}

/*******************************************************************************
 * Ubsługa czasu gry:
 * - przesuwanie klocka w dół
 * - sprawdzanie czy klocek dotarł do celu
 *
 *
 * Ta funkcja jest wywoływana synchronicznie przy pomocy timera systemowego.
 * Po wykonaniu wszsytkoch zadan timer jest odpalany ponownie (add_timer_task),
 * tak aby utrzymac synchroniczność dzałania.
 * -
 */
void gra_tick(void)
{
   int i;
   int j;
   int najwyzsza_linia;

   /* podbij licznik ruchów klocka - uzywany jest on przy wykrywaniu, czy klocki
    * nie odkładają się już na samej górze planysz (ta sytuacja oznacza koniec
    * gry) */
   ruchy++;

   /* sprawdz czy trzeba dodać nowy klocek lub czy trzeba usunąć linie */
   if(jest_klocek == 0)
   {
      /* szukaj całych linii */
      szukaj_linii();

      /* dodaj nowy klocek na planszę */
      dodaj_klocek();
      jest_klocek = 1;
   }
   else
   {
         /* sprawdź czy klocek dojechał na dół lub czy gra nie dobiegła końca */
         if(sprawdz_klocek() != 0)
         {
            /* koniec gry */
            koniec_gry();
         }
         else
         {


         /* sprawdź czy jest jakaś kompletna linia */

         /* przesuń klocki w dół */
         najnizsza_linia++;

         /* kopiujemy tylko linie w otoczeniu obecnego klocka */
         najwyzsza_linia = najnizsza_linia - 17;
         if(najwyzsza_linia<0) najwyzsza_linia = 0;


         for(i = LCD_WYSOKOSC-1; i>=1; i--)
         {
            for(j = LCD_SZEROKOSC-1; j>=najwyzsza_linia; j--)
            {
               klocek[i][j] = klocek[i][j-1];
            }
         }


         /* uaktualnienie planszy - nałóż na siebie plansze i obecną pozycję
          * klocka */
         for (i=0; i< LCD_WYSOKOSC; i++)
         {
            for (j=0; j< LCD_SZEROKOSC; j++)
            {
               klocki[i][j] = klocek[i][j] | plansza[i][j];
            }
         }

      }
   }
   /* wystartuj nowy timer */
   rysuj_ekran(&piksele[0][0]);
   add_timer_task(TIMER_FREQUENCY_HZ/4);
}

/*******************************************************************************
 * Inicjalizacja PINu PA1 do obsługi przerwania od przycisku.
 * Pin z podciągnięciem do VCC, reaguje na zbocze narastające.
 */
void init_PA1 (void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

   /* uruchomienie zegara dla tego pinu */
   RCC->AHB1ENR |= BLINK_RCC_MASKx(0);

   /* parametry pinu */
   GPIO_InitStructure.Pin = GPIO_PIN_1;
   GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
   GPIO_InitStructure.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

   //uruchomienie przerwań zewnętrznych dla EXTI1
   NVIC_EnableIRQ(EXTI1_IRQn);

}

/*******************************************************************************
 * Inicjalizacja PINu PA2 do obsługi przerwania od przycisku.
 * Pin z podciągnięciem do VCC, reaguje na zbocze narastające.
 */
void init_PA2 (void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

   /* uruchomienie zegara dla tego pinu */
   RCC->AHB1ENR |= BLINK_RCC_MASKx(0);

   /* parametry pinu */
   GPIO_InitStructure.Pin = GPIO_PIN_2;
   GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
   GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
   GPIO_InitStructure.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

   //uruchomienie przerwań zewnętrznych dla EXTI2
   NVIC_EnableIRQ(EXTI2_IRQn);
}

/*******************************************************************************
 * Inicjalizacja PINu PA3 do obsługi przerwania od przycisku.
 * Pin z podciągnięciem do VCC, reaguje na zbocze narastające.
 */
void init_PA3 (void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

   /* uruchomienie zegara dla tego pinu */
   RCC->AHB1ENR |= BLINK_RCC_MASKx(0);

   /* parametry pinu */
   GPIO_InitStructure.Pin = GPIO_PIN_3;
   GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
   GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
   GPIO_InitStructure.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

   //uruchomienie przerwań zewnętrznych dla EXTI3
   NVIC_EnableIRQ(EXTI3_IRQn);
}

/*******************************************************************************
 * Inicjalizacja PINu PA4 do obsługi przerwania od przycisku.
 * Pin z podciągnięciem do VCC, reaguje na zbocze narastające.
 */
void init_PA4 (void)
{
   GPIO_InitTypeDef GPIO_InitStructure;

   /* uruchomienie zegara dla tego pinu */
   RCC->AHB1ENR |= BLINK_RCC_MASKx(0);

   /* parametry pinu */
   GPIO_InitStructure.Pin = GPIO_PIN_4;
   GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
   GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
   GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
   GPIO_InitStructure.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

   //uruchomienie przerwań zewnętrznych dla EXTI4
   NVIC_EnableIRQ(EXTI4_IRQn);
}


/*
 * Obsługa przerwania od przycisku EXTI1 - PA1
 * Przesuwanie w lewo
 */
void EXTI1_IRQHandler(void)
{
   int i;
   int j;

   /* wyczyść flagę przerwania */
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);

   if(lewa_linia > 1)
   {
      lewa_linia--;
      prawa_linia--;

      /* przesuń klocek w lewo */
      for(i = LCD_WYSOKOSC-2; i>1; i--)
      {
         for(j=LCD_SZEROKOSC-1; j>=0; j--)
         {
            klocek[i][j] = klocek[i-1][j];
         }
      }

      /* dodatkowo przypadek kiedy byliśmy przy prawej krawędzi
       * W tym przypadku, musimy dodatoko wyczyścić boczną krawędź klocka
       * (odczepić go od ściany) */
      if(prawa_linia == 46)
      {
         for(j = LCD_SZEROKOSC-1; j>=0; j--)
         {
            klocek[1][j] = 0;
         }
      }
   }

}

/*******************************************************************************
 * Obsługa przerwania od przycisku EXTI2 - PA2
 * Ten guzik jest używany do przesuwania klocków w prawo
 */
void EXTI2_IRQHandler(void)
{
   int i;
   int j;

   /* wyczyść flagę przerwania */
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);

   if(prawa_linia < LCD_WYSOKOSC-1)
   {
      /* zaktualizuj krawędzie boczne klocka */
      lewa_linia++;
      prawa_linia++;

      /* przesuń klocek w prawo */
      for(i = 1; i< LCD_WYSOKOSC-1; i++)
      {
         for(j = LCD_SZEROKOSC-1; j>=0; j--)
         {
            klocek[i][j] = klocek[i+1][j];
         }
      }

      /* dodatkowo przypadek kiedy byliśmy przy lewej krawędzi
       * W tym przypadku, musimy dodatoko wyczyścić boczną krawędź klocka
       * (odczepić go od ściany) */
      if(lewa_linia == 2)
      {
         for(j = LCD_SZEROKOSC-1; j>=0; j--)
         {
            klocek[LCD_WYSOKOSC-1][j] = 0;
         }
      }
   }
}

/*******************************************************************************
 * Obsługa przerwania od przycisku EXTI3 - PA3
 * Ten guzik jest używany do obracania klocków
 */
void EXTI3_IRQHandler(void)
{
   int i;
   int j;

   /* wyczyść flagę przerwania */
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);

   /* aktualnie pbracać można tylko prostokątem (nie ma sensu obracać kwadratem,
    * a inne klocki nie sa używane */
   if(biezacy_klocek == PROSTOKAT)
   {
      /* sprawdź czy klocek jest aktualnie w orientacji pionowej */
      if(pozycja_klocka == PROSTOKAT_PION)
      {
         /* zaktualizuj orientację klocka */
         pozycja_klocka = PROSTOKAT_POZIOM;

         /* wyczyść pionową część */
         for(i=LCD_WYSOKOSC - lewa_linia - 2;
             i<LCD_WYSOKOSC - lewa_linia;
             i++)
         {
            for(j=najnizsza_linia-16;
                j<najnizsza_linia-2;
                j++)
            {
               klocek[i][j] = 0;
            }
         }

         /* dodaj poziomą część */
         for(i=LCD_WYSOKOSC - lewa_linia - 16;
             i<LCD_WYSOKOSC - lewa_linia;
             i++)
         {
            for(j=najnizsza_linia-2;
                j<najnizsza_linia;
                j++)
            {
               klocek[i][j] = 1;
            }
         }

         /* zaktualizuj prawą krawędź klocka */
         prawa_linia += 14;

      }
      /* sprawdź czy klocek jest aktualnie w orientacji pociomej */
      else if(pozycja_klocka == PROSTOKAT_POZIOM)
      {
         /* zaktualizuj orientację klocka */
         pozycja_klocka = PROSTOKAT_PION;

         /* wyczyść poziomą część */
         for(i=LCD_WYSOKOSC - lewa_linia - 16;
             i<LCD_WYSOKOSC - lewa_linia;
             i++)
         {
            for(j=najnizsza_linia-2;
                j<najnizsza_linia;
                j++)
            {
               klocek[i][j] = 0;
            }
         }

         /* dodaj pionową część */
         for(i=LCD_WYSOKOSC - lewa_linia - 2;
             i<LCD_WYSOKOSC - lewa_linia;
             i++)
         {
            for(j=najnizsza_linia-16;
                j<najnizsza_linia;
                j++)
            {
               klocek[i][j] = 1;
            }
         }

         /* zaktualizuj prawą krawędź klocka */
         prawa_linia -= 14;
      }
   }

}

/*******************************************************************************
 * Obsługa przerwania od przycisku EXTI4 - PA4
 * Ten guzik jest używany do startowania gry.
 */
void EXTI4_IRQHandler(void)
{
   /* wyczyść flagę przerwania */
   HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);

   start_gry = 1;

}

/*******************************************************************************
 * Główna funckja programu
 * - inicjalizacja pracy systemu
 * - start gry
 * - inkrementacja zmiennej 'losowy_licznik'
 */
int main(int argc, char* argv[])
{

  /*
   * Inicjalizacja systemu
   * PA1 - przewanie od przycisku
   * PA2 - przewanie od przycisku
   * PA3 - przewanie od przycisku
   * PA4 - przewanie od przycisku
   * Inicjalizacja LCD
   */
  init_PA1();
  init_PA2();
  init_PA3();
  init_PA4();

  /* inicjalizacja timera */
  timer_start();

  /* inicjalizacja diod */
  blink_led_init();

  /* uruchomienie wyświetlacza */
  LCD_INIT();

  /* główna pętla programu */
 while (1)
 {
     /* ten licznik jest uzywany do 'losowania' nowych klocków
      * Jest on inkrementowany w czasie gdy układ nie ma nic innego do roboty */
     losowy_licznik++;
 }

}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
