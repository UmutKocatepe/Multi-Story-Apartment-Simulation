#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>

#define KAT_SAYISI 10
#define DAIRE_SAYISI 4

typedef struct {
    int insaat;
    int boya;
    int mobilya;
    int internet;
} KatDurumu;

sem_t sem_vinc;
sem_t sem_asansor;

KatDurumu *kat_durumlari;
pthread_mutex_t vinç_mutex, asansör_mutex, boya_mutex, mobilya_mutex, internet_mutex;
volatile sig_atomic_t deprem_var = 0;
volatile sig_atomic_t yangin_var = 0;
int *kat_bitti;
int *boya_bitti;

// Yangin kontrol thread’i: Belirli sure sonra yangin cikar ve digr islemleri gecici durdurur
void *yangin_kontrol(void *arg) {
    if (rand() % 100 < 70) {  // %70 olasilikla yangin cikmaz
        pthread_exit(NULL);
    }

    sleep(rand() % 20 + 5); // Yangin gecikmesi

    int daire = rand() % DAIRE_SAYISI + 1;

    printf("\n[YANGIN] Daire %d'de yangin cikti . Tum isler gecici olarak durduruluyor!\n", daire);
    fflush(stdout);
    yangin_var = 1;

    sleep(0.5); // Müdahale süresi
    printf("[YANGIN] Yangin sonduruldu, isler devam ediyor.\n");
    fflush(stdout);
    yangin_var = 0;

    pthread_exit(NULL);
}


// Deprem kontrol thread’i: %65 olasilikla olmaz, olursa siddetine gore islem yapilir
void *deprem_kontrol(void *arg) {
    if (rand() % 100 < 65) {
        // %65 olasilikla hic deprem olmaz
        pthread_exit(NULL);
    }

    sleep(rand() % 15 + 5); // Depremin meydana geldigi zamanisimule et

    float siddet;
    int olasilik = rand() % 100;

    if (olasilik < 60) {
        // %60 olasilikla 3.0–4.9
        siddet = ((rand() % 20) + 30) / 10.0;
    } else if (olasilik < 90) {
        // %30 olasilikla 5.0–6.4
        siddet = ((rand() % 15) + 50) / 10.0;
    } else {
        // %10 olasilikla 6.5–7.5
        siddet = ((rand() % 11) + 65) / 10.0;
    }

    printf("\n[DEPREM] %.1f siddetinde deprem meydana geldi!\n", siddet);
    fflush(stdout);
    deprem_var = 1;

    if (siddet >= 6.5) {
        int kayip = rand() % (KAT_SAYISI * DAIRE_SAYISI) + 1;
        printf("[YIKIM] Bina agir hasar aldı ve coktu! %d kisi hayatini kaybetti.\n", kayip);
        printf("Insaat tamamen iptal edildi. Sistem kapatiliyor...\n");
        fflush(stdout);
        kill(0, SIGKILL);
    } else if (siddet >= 5.0) {
        printf("[HASAR] Bina orta duzeyde hasar aldi. Guvenlik denetimi yapilmali.\n");

        sleep(0.5); // guvenlik denetimi icin bekleme
        printf("[DEPREM] Guvenlik kontrolu tamamlandi. Isler devam ediyor.\n");
        fflush(stdout);
        deprem_var = 0;
    } else {
        printf("[GUVENLİK] Bina depremi hasarsiz atlatti. Insaata devam ediliyor.\n");
        fflush(stdout);
        deprem_var = 0;
    }

    pthread_exit(NULL);
}


// Su tesisati islemi: Her dairede baslatilir, yangin varsa beklenir
void *su_tesisati(void *arg) {
    int su_tesisati_daire = *(int *)arg;
    free(arg);
    printf("[Daire %d] Su tesisati basliyor...\n", su_tesisati_daire);
    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);
    printf("[Daire %d] Su tesisati tamamlandi.\n", su_tesisati_daire);
    pthread_exit(NULL);
}

// Elektrik tesisati islemi: Su tesisatindan sonra baslatilir
void *elektrik_tesisati(void *arg) {
    int elektrik_tesisat_daire = *(int *)arg;
    free(arg);
    printf("[Daire %d] Elektrik tesisati basliyor...\n",elektrik_tesisat_daire);
    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);
    printf("[Daire %d] Elektrik tesisati tamamlandi.\n",elektrik_tesisat_daire);
    pthread_exit(NULL);
}

// Zemin doseme işlemi: Su ve elektrik tamamlandiktan sonra yapilir
void *zemin_dose(void *arg) {
    int zemin_dose_daire = *(int *)arg;
    free(arg);
    printf("[Daire %d] Zemin doseme basliyor...\n",zemin_dose_daire);
    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);
    printf("[Daire %d] Zemin doseme tamamlandi.\n",zemin_dose_daire);
    pthread_exit(NULL);
}

// Kapı montaji islemi
void *kapi_montaj(void *arg) {
    int kapi_montaj_daire = *(int *)arg; //
    free(arg); //
    printf("[Daire %d] Kapi montaji basliyor...\n",kapi_montaj_daire);
    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);
    printf("[Daire %d] Kapi montaji tamamlandi.\n",kapi_montaj_daire);
    pthread_exit(NULL);
}

// Cam takma islemi
void *cam_tak(void *arg) {
    int cam_daire_no = *(int *)arg; //
    free(arg);  //
    printf("[Daire %d] Cam takma basliyor...\n", cam_daire_no);
    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);
    printf("[Daire %d] Cam takma tamamlandi.\n",cam_daire_no);
    pthread_exit(NULL);
}

// Daire insa fonksiyonu: 1 dairenin tüm alt islemleri sirasiyla yapilir
void *daire_insa(void *arg) {
    sleep(0.2);

    int daire_no = *(int *)arg;
    free(arg);
    printf("[Daire %d] Insa sureci basladi.\n", daire_no);

    pthread_t su_tid, el_tid, zemin_tid, kapi_tid, cam_tid;

    int *su_arg = malloc(sizeof(int));
    *su_arg = daire_no;
    pthread_create(&su_tid, NULL, su_tesisati, su_arg);
    pthread_join(su_tid, NULL);

    int *el_arg = malloc(sizeof(int));
    *el_arg = daire_no;
    pthread_create(&el_tid, NULL, elektrik_tesisati, el_arg);
    pthread_join(el_tid, NULL);

    int *zemin_arg = malloc(sizeof(int));
    *zemin_arg = daire_no;
    pthread_create(&zemin_tid, NULL, zemin_dose, zemin_arg);
    pthread_join(zemin_tid, NULL);

    int *kapi_arg = malloc(sizeof(int));
    *kapi_arg = daire_no;
    pthread_create(&kapi_tid, NULL, kapi_montaj, kapi_arg);
    pthread_join(kapi_tid, NULL);

    int *cam_arg = malloc(sizeof(int));
    *cam_arg = daire_no;
    pthread_create(&cam_tid, NULL, cam_tak, cam_arg);
    pthread_join(cam_tid, NULL);

    while (yangin_var || deprem_var) usleep(100000);
    sleep(1);  // 1 saniyelik sivama islem suresi simule edilir

    printf("[Daire %d] Insa sureci tamamlandi.\n", daire_no);
    pthread_exit(NULL);
}


// Kat boyama islemi: Insaat bitince baslar, mutex ile senkronize
void *boya_kat_thread(void *arg) {
    int kat = *(int *)arg;
    free(arg);
    while (!kat_bitti[kat - 1]) usleep(100000);
    while (yangin_var || deprem_var) usleep(100000);

    pthread_mutex_lock(&boya_mutex);
    printf("[BOYA] Kat %d boyaniyor.\n", kat);
    sleep(1);
    pthread_mutex_unlock(&boya_mutex);

    boya_bitti[kat - 1] = 1;
    kat_durumlari[kat - 1].boya = 1;
    printf("[BOYA] Kat %d boyama tamamlandi.\n", kat);
    pthread_exit(NULL);
}

// Mobilya tasima işlemi: Her kata paralel olarak yapilir
void *mobilya_tasi(void *arg) {
    int kat = *(int *)arg;
    free(arg);
    while (yangin_var || deprem_var) usleep(100000);

    pthread_mutex_lock(&mobilya_mutex);
    printf("[Mobilya] Kat %d mobilya tasima basladi.\n", kat);
    sleep(1);
    printf("[Mobilya] Kat %d mobilya yerlestirildi.\n", kat);
    pthread_mutex_unlock(&mobilya_mutex);

    kat_durumlari[kat - 1].mobilya = 1;
    pthread_exit(NULL);
}

// Internet baglanti islemi: Her kata ayri ayri uygulanır
void *internet_bagla(void *arg) {
    int kat = *(int *)arg;
    free(arg);
    while (yangin_var || deprem_var) usleep(100000);

    pthread_mutex_lock(&internet_mutex);
    printf("[Internet] Kat %d baglanti kuruluyor...\n", kat);
    sleep(1);
    printf("[Internet] Kat %d baglanti tamamlandi.\n", kat);
    pthread_mutex_unlock(&internet_mutex);

    kat_durumlari[kat - 1].internet = 1;
    pthread_exit(NULL);
}

// Katlarin insaat, boya, mobilya, internet durumlarini yazdirir
void rapor_yazdir() {
    printf("\n[KAT DURUM RAPORU]\n");
    printf("Kat | Insaat | Boya | Mobilya | Internet\n");
    printf("----|--------|------|---------|---------\n");
    for (int i = 0; i < KAT_SAYISI; i++) {
        printf(" %2d |   %s   | %s  |   %s   |   %s\n",
               i + 1,
               kat_durumlari[i].insaat ? "✓" : " ✗ ",
               kat_durumlari[i].boya ? " ✓ " : " ✗ ",
               kat_durumlari[i].mobilya ? " ✓ " : " ✗ ",
               kat_durumlari[i].internet ? " ✓ " : " ✗ ");
    }
}

// Kat insaat fonksiyonu:
// Bu fonksiyon , katin tüm dairelerini insa etmek icin cagrilir.
// fork() edilmis process icinde her daireyi paralel thread olarak insa eder
void kat_insa_et(int kat_no) {
    sleep(0.5);
    srand(time(NULL) + getpid()); // Her alt process'in benzersiz rand() dizisine sahip olmasi icin
    printf("\n=== [Kat %d] Insaat Basladi ===\n", kat_no);

    pthread_t daireler[DAIRE_SAYISI];

    for (int i = 0; i < DAIRE_SAYISI; i++) {
        int *daire_no = malloc(sizeof(int));
        *daire_no = i + 1;
        pthread_create(&daireler[i], NULL, daire_insa, daire_no);
    }

    for (int i = 0; i < DAIRE_SAYISI; i++) {
        pthread_join(daireler[i], NULL);
    }

    if (rand() % 100 < 10) {
        printf("‼[KAZA] Kat %d’de isci dustu! 2 saniye duraklama...\n", kat_no);
        sleep(2);
    }

    sem_wait(&sem_asansor);
    printf("[ASANSOR] Kat %d asansor alani kullaniyor...\n", kat_no);
    sleep(0.5);
    sem_post(&sem_asansor);

    sem_wait(&sem_vinc);
    printf("[VINC] Kat %d vinc ile malzeme çekiyor...\n", kat_no);
    sleep(0.5);
    sem_post(&sem_vinc);

    kat_bitti[kat_no - 1] = 1;
    kat_durumlari[kat_no - 1].insaat = 1;
    printf("=== [Kat %d] Insaat Bitti ===\n", kat_no);
    exit(0);
}

// Ana fonksiyon: Tum sistemi baslatir, tum islemleri sirasiyla koordine eder
int main() {
    srand(time(NULL));

    // mutex'lerin baslatilmasi
    pthread_mutex_init(&vinç_mutex, NULL);
    pthread_mutex_init(&asansör_mutex, NULL);
    pthread_mutex_init(&boya_mutex, NULL);
    pthread_mutex_init(&mobilya_mutex, NULL);
    pthread_mutex_init(&internet_mutex, NULL);

    sem_init(&sem_vinc, 1, 2); // semaforların baslatilmasi
    sem_init(&sem_asansor, 1, 1);

    // Paylasilan bellekler(Shared Memory) : mmap() ile paylasilan bellekler tahsis edilir .
    // Bu sayede birden fazla process veya thread ayni fiziksel bellek alanina erisebilir . Veri kopyalamadan senkronize bilgi paylasimi yapilabilir.
    kat_bitti = mmap(NULL, KAT_SAYISI * sizeof(int), PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    boya_bitti = mmap(NULL, KAT_SAYISI * sizeof(int), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    kat_durumlari = mmap(NULL, KAT_SAYISI * sizeof(KatDurumu), PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    char secim;
    do {
        for (int i = 0; i < KAT_SAYISI; i++) { // Tüm katlarin(10 kat) durumlari sifirlanır . Onceden insaat yapildiysa temizlenir .
            kat_bitti[i] = 0;
            boya_bitti[i] = 0;
            kat_durumlari[i].insaat = 0;
            kat_durumlari[i].boya = 0;
            kat_durumlari[i].mobilya = 0;
            kat_durumlari[i].internet = 0;
        }

        printf("\n>>> [BASLANGIC] Apartman insaati basliyor...\n");

        // Paralel olarak deprem ve yangin olasiliklarini yoneten iki ayri thread baslatilir
        pthread_t deprem_tid, yangin_tid;
        pthread_create(&deprem_tid, NULL, deprem_kontrol, NULL);
        pthread_create(&yangin_tid, NULL, yangin_kontrol, NULL);

        pid_t pids[KAT_SAYISI];
        for (int i = 1; i <= KAT_SAYISI; i++) {
            pids[i - 1] = fork(); // Her kat icin fork() ile child process olusturulur.
            if (pids[i - 1] == 0) kat_insa_et(i); // kat_insa_et(i) fonksiyonu sadece cocuk process icinde calisir.
            else wait(NULL); //Ana process her katin tamamlanmasini sirayla bekler . Katlar paralel degil , ardisik insa edilir.
        }

        // Insaati biten her kat icin boyama islemi baslatilir
        // Boyama islemi her kat icin ayri bir thread ile yapilir.
        pthread_t boya_threads[KAT_SAYISI];
        int katlar[KAT_SAYISI];
        for (int i = 0; i < KAT_SAYISI; i++) {
            int *kat_no = malloc(sizeof(int));
            *kat_no = i + 1;
            pthread_create(&boya_threads[i], NULL, boya_kat_thread, kat_no);
        }
        for (int i = 0; i < KAT_SAYISI; i++) {
            pthread_join(boya_threads[i], NULL); // Tum boyama islemleri tamamlanana kadar beklenir
        }

        printf("\n>>> [İSLEM] Boyama tamamlandi. Mobilya ve internet islemleri baslatiliyor...\n");

        // Her kat icin mobilya ve internet baglantisi islemi icin ayri birer thread baslatilir
        pthread_t mobilya_threads[KAT_SAYISI];
        pthread_t internet_threads[KAT_SAYISI];
        for (int i = 0; i < KAT_SAYISI; i++) {
            int *katno1 = malloc(sizeof(int));
            *katno1 = i + 1;
            pthread_create(&mobilya_threads[i], NULL, mobilya_tasi, katno1);

            int *katno2 = malloc(sizeof(int));
            *katno2 = i + 1;
            pthread_create(&internet_threads[i], NULL, internet_bagla, katno2);
        }

        for (int i = 0; i < KAT_SAYISI; i++) { // Tum mobilya tasima ve internet baglanti islemlerinin bitmesi beklenir
            pthread_join(mobilya_threads[i], NULL);
            pthread_join(internet_threads[i], NULL);
        }

        //Katlar islemleri bittikten sonra afet thread'leri iptal edilir.
        pthread_cancel(deprem_tid);
        pthread_cancel(yangin_tid);

        printf("\n[TAMAMLANDI] Apartman hazir! Yasanabilir durumda.\n");
        rapor_yazdir();

        printf("\nApartmani yeniden insa etmek ister misiniz? (E/H): ");
        scanf(" %c", &secim);

    } while (secim == 'E' || secim == 'e');

    //Tum kaynaklar(mutexler, semaforlar, paylasilan bellekler) serbest birakilir
    pthread_mutex_destroy(&vinç_mutex);
    pthread_mutex_destroy(&asansör_mutex);
    pthread_mutex_destroy(&boya_mutex);
    pthread_mutex_destroy(&mobilya_mutex);
    pthread_mutex_destroy(&internet_mutex);

    sem_destroy(&sem_vinc);
    sem_destroy(&sem_asansor);

    munmap(kat_bitti, KAT_SAYISI * sizeof(int));
    munmap(boya_bitti, KAT_SAYISI * sizeof(int));
    munmap(kat_durumlari, KAT_SAYISI * sizeof(KatDurumu));

    printf("\nProgram sonlandirildi. İyi çalismalar!\n");
    return 0;
}
