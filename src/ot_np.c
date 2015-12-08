/*
 * Implementation of semi-honest OT as detailed by Naor and Pinkas [1].
 *
 * Author: Alex J. Malozemoff <amaloz@cs.umd.edu>
 *
 * [1] "Efficient Oblivious Transfer Protocols."
 *     N. Naor, B. Pinkas. SODA 2001.
 */
#include "ot_np.h"

#include "crypto.h"
#include "log.h"
#include "net.h"
#include "state.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmp.h>
#include <openssl/sha.h>
#include "aes.h"

#define SHA

#if !defined AES_HW && !defined SHA
#error one of AES_HW, SHA must be defined
#endif

#define ERROR { err = 1; goto cleanup; }

/*
 * Runs sender operations for Naor-Pinkas semi-honest OT
 */
int
ot_np_send(struct state *st, int fd, void *msgs, int msglength, int num_ots,
           int N, ot_msg_reader ot_msg_reader, ot_item_reader ot_item_reader)
{
    mpz_t r, gr, pk, pk0;
    mpz_t *Cs = NULL, *Crs = NULL, *pk0s = NULL;
    char buf[field_size], *msg = NULL;
    int err = 0;
    double start, end;

#ifdef AES_HW
    //fprintf(stderr, "OT-NP: Using AESNI\n");
#endif
#ifdef SHA
    //fprintf(stderr, "OT-NP: Using SHA-1\n");
#endif

    start = current_time();

    mpz_inits(r, gr, pk, pk0, NULL);

    msg = (char *) ot_malloc(sizeof(char) * msglength);
    if (msg == NULL)
        ERROR;
    Cs = (mpz_t *) ot_malloc(sizeof(mpz_t) * (N - 1));
    if (Cs == NULL)
        ERROR;
    Crs = (mpz_t *) ot_malloc(sizeof(mpz_t) * (N - 1));
    if (Crs == NULL)
        ERROR;
    pk0s = (mpz_t *) ot_malloc(sizeof(mpz_t) * num_ots);
    if (pk0s == NULL)
        ERROR;

    for (int i = 0; i < num_ots; ++i) {
        mpz_init(pk0s[i]);
    }

#ifdef AES_HW
    AES_KEY key;
    AES_set_encrypt_key((unsigned char *) "abcd", 128, &key);
#endif

    // choose r \in_R Zq
    random_element(r, &st->p);
    // compute g^r
    mpz_powm(gr, st->p.g, r, st->p.p);

    // choose C_i's \in_R Zq
    for (int i = 0; i < N - 1; ++i) {
        mpz_inits(Cs[i], Crs[i], NULL);
        random_element(Cs[i], &st->p);
    }

    end = current_time();
    //fprintf(stderr, "Initialization: %f\n", end - start);

    // send g^r to receiver
    start = current_time();
    mpz_to_array(buf, gr, sizeof buf);
    if (net_send(fd, buf, sizeof buf, 0) == -1)
        ERROR;
    end = current_time();
    //fprintf(stderr, "Send g^r to receiver: %f\n", end - start);

    // send Cs to receiver
    start = current_time();
    for (int i = 0; i < N - 1; ++i) {
        mpz_to_array(buf, Cs[i], sizeof buf);
        if (net_send(fd, buf, sizeof buf, 0) == -1)
            ERROR;
    }
    end = current_time();
    //fprintf(stderr, "Send Cs to receiver: %f\n", end - start);

    start = current_time();
    for (int i = 0; i < N - 1; ++i) {
        // compute C_i^r
        mpz_powm(Crs[i], Cs[i], r, st->p.p);
    }
    end = current_time();
    //fprintf(stderr, "Compute C_i^r: %f\n", end - start);

    start = current_time();
    for (int j = 0; j < num_ots; ++j) {
        // get pk0 from receiver
        if (net_recv(fd, buf, sizeof buf, 0) == -1)
            ERROR;
        array_to_mpz(pk0s[j], buf, sizeof buf);
    }
    end = current_time();
    //fprintf(stderr, "Get pk0 from receiver: %f\n", end - start);

    start = current_time();
    for (int j = 0; j < num_ots; ++j) {
        void *ot = ot_msg_reader(msgs, j);
        for (int i = 0; i < N; ++i) {
            char *item;
            ssize_t itemlength;

            if (i == 0) {
                // compute pk0^r
                mpz_powm(pk0, pk0s[j], r, st->p.p);
                mpz_to_array(buf, pk0, sizeof buf);
                (void) mpz_invert(pk0, pk0, st->p.p);
            } else {
                mpz_mul(pk, pk0, Crs[i - 1]);
                mpz_mod(pk, pk, st->p.p);
                mpz_to_array(buf, pk, sizeof buf);
            }

#ifdef AES_HW
            if (AES_encrypt_message((unsigned char *) buf, sizeof buf,
                                    (unsigned char *) msg, msglength, &key))
                ERROR;
#endif
#ifdef SHA
            (void) memset(msg, '\0', msglength);
            sha1_hash(msg, msglength, i, (unsigned char *) buf, sizeof buf);
#endif            

            item = (char *) ot_item_reader(ot, i, &itemlength);
            assert(itemlength <= msglength);

            xorarray((unsigned char *) msg, msglength,
                     (unsigned char *) item, itemlength);
            if (net_send(fd, msg, msglength, 0) == -1)
                ERROR;
        }
    }
    end = current_time();
    //fprintf(stderr, "Send hashes to receiver: %f\n", end - start);

 cleanup:
    mpz_clears(r, gr, pk, pk0, NULL);

    if (pk0s) {
        for (int i = 0; i < num_ots; ++i) {
            mpz_clear(pk0s[i]);
        }
        ot_free(pk0s);
    }
    if (Crs) {
        for (int i = 0; i < N - 1; ++i)
            mpz_clear(Crs[i]);
        ot_free(Crs);
    }
    if (Cs) {
        for (int i = 0; i < N - 1; ++i)
            mpz_clear(Cs[i]);
        ot_free(Cs);
    }
    if (msg)
        ot_free(msg);

    return err;
}

int
ot_np_recv(struct state *st, int fd, void *choices, int nchoices, int msglength,
           int N, void *out,
           ot_choice_reader ot_choice_reader, ot_msg_writer ot_msg_writer)
{
    mpz_t gr, pk0, pks;
    mpz_t *Cs = NULL, *ks = NULL;
    char buf[field_size], *from = NULL, *msg = NULL;
    int err = 0;
    double start, end;

#ifdef AES_HW
    //fprintf(stderr, "OT-NP: Using AESNI\n");
#endif
#ifdef SHA
    //fprintf(stderr, "OT-NP: Using SHA-1\n");
#endif

    mpz_inits(gr, pk0, pks, NULL);

    msg = (char *) ot_malloc(sizeof(char) * msglength);
    if (msg == NULL)
        ERROR;
    from = (char *) ot_malloc(sizeof(char) * msglength);
    if (from == NULL)
        ERROR;
    Cs = (mpz_t *) ot_malloc(sizeof(mpz_t) * (N - 1));
    if (Cs == NULL)
        ERROR;
    for (int i = 0; i < N - 1; ++i) {
        mpz_init(Cs[i]);
    }
    ks = (mpz_t *) ot_malloc(sizeof(mpz_t) * nchoices);
    if (ks == NULL)
        ERROR;
    for (int j = 0; j < nchoices; ++j) {
        mpz_init(ks[j]);
    }

#ifdef AES_HW
    AES_KEY key;
    AES_set_encrypt_key((unsigned char *) "abcd", 128, &key);
#endif

    // get g^r from sender
    start = current_time();
    if (net_recv(fd, buf, sizeof buf, 0) == -1)
        ERROR;
    array_to_mpz(gr, buf, sizeof buf);
    end = current_time();
    //fprintf(stderr, "Get g^r from sender: %f\n", end - start);

    // get Cs from sender
    start = current_time();
    for (int i = 0; i < N - 1; ++i) {
        if (net_recv(fd, buf, sizeof buf, 0) == -1)
            ERROR;
        array_to_mpz(Cs[i], buf, sizeof buf);
    }
    end = current_time();
    //fprintf(stderr, "Get Cs from sender: %f\n", end - start);

    start = current_time();
    for (int j = 0; j < nchoices; ++j) {
        long choice;

        choice = ot_choice_reader(choices, j);
        assert(choice == 0 || choice == 1);
        // choose random k
        mpz_urandomb(ks[j], st->p.rnd, sizeof buf * 8);
        mpz_mod(ks[j], ks[j], st->p.q);
        // compute pks = g^k
        mpz_powm(pks, st->p.g, ks[j], st->p.p);
        // compute pk0 = C_1 / g^k regardless of whether our choice is 0 or 1 to
        // avoid a potential side-channel attack
        (void) mpz_invert(pk0, pks, st->p.p);
        mpz_mul(pk0, pk0, Cs[0]);
        mpz_mod(pk0, pk0, st->p.p);
        mpz_set(pk0, choice == 0 ? pks : pk0);
        mpz_to_array(buf, pk0, sizeof buf);
        // send pk0 to sender
        if (net_send(fd, buf, sizeof buf, 0) == -1)
            ERROR;
    }
    end = current_time();
    //fprintf(stderr, "Send pk0s to sender: %f\n", end - start);

    start = current_time();
    for (int j = 0; j < nchoices; ++j) {
        long choice;

        choice = ot_choice_reader(choices, j);
        assert(choice == 0 || choice == 1);

        // compute decryption key (g^r)^k
        mpz_powm(ks[j], gr, ks[j], st->p.p);

        for (int i = 0; i < N; ++i) {
            mpz_to_array(buf, ks[j], sizeof buf);
            // get H xor M0 from sender
            if (net_recv(fd, msg, msglength, 0) == -1)
                ERROR;

#ifdef AES_HW
            if (AES_encrypt_message((unsigned char *) buf, sizeof buf,
                                    (unsigned char *) from, msglength, &key))
                ERROR;
#endif
#ifdef SHA
            (void) memset(from, '\0', msglength);
            sha1_hash(from, msglength, i, (unsigned char *) buf, sizeof buf);
#endif

            xorarray((unsigned char *) msg, msglength,
                     (unsigned char *) from, msglength);

            if (i == choice) {
                ot_msg_writer(out, j, msg, msglength);
            }
        }
    }
    end = current_time();
    //fprintf(stderr, "Receive hashes from sender: %f\n", end - start);

 cleanup:
    mpz_clears(gr, pk0, pks, NULL);

    if (ks) {
        for (int j = 0; j < nchoices; ++j) {
            mpz_clear(ks[j]);
        }
        ot_free(ks);
    }
    if (Cs) {
        for (int i = 0; i < N - 1; ++i)
            mpz_clear(Cs[i]);
        ot_free(Cs);
    }
    if (msg)
        ot_free(msg);
    if (from)
        ot_free(from);

    return err;
}
