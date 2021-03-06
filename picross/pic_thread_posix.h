/*
 Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com

 This file is part of EigenD.

 EigenD is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 EigenD is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __PICROSS_PIC_THREAD__
#define __PICROSS_PIC_THREAD__

#include <picross/pic_error.h>
#include <picross/pic_atomic.h>
#include <picross/pic_config.h>

#ifdef PI_MACOSX 
#include <mach/mach.h>
#else
#include <semaphore.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "pic_exports.h"

typedef pthread_t pic_threadid_t;

PIC_DECLSPEC_FUNC(void) pic_set_fpu();
PIC_DECLSPEC_FUNC(void) pic_set_foreground(bool rt);
PIC_DECLSPEC_FUNC(void) pic_set_interrupt(void);
PIC_DECLSPEC_FUNC(void) pic_mlock_code(void);
PIC_DECLSPEC_FUNC(void) pic_set_core_unlimited();
PIC_DECLSPEC_FUNC(void) pic_init_dll_path(void);

PIC_DECLSPEC_FUNC(void) pic_thread_lck_free(void *ptr, unsigned size);
PIC_DECLSPEC_FUNC(void) *pic_thread_lck_malloc(unsigned size);
PIC_DECLSPEC_FUNC(void) pic_thread_yield();

PIC_DECLSPEC_FUNC(pic_threadid_t) pic_current_threadid();
PIC_DECLSPEC_FUNC(bool) pic_threadid_equal(pic_threadid_t, pic_threadid_t);

namespace pic
{
    struct logger_t;
    struct nballocator_t;

    class PIC_DECLSPEC_CLASS semaphore_t
    {
        public:
            semaphore_t();
            ~semaphore_t();
            void up();
            bool untimeddown();
            bool timeddown(unsigned long long timeout);
        private:
#ifdef PI_MACOSX
            ::semaphore_t sem_;
#else
            sem_t sem_;
#endif
    };

    class PIC_DECLSPEC_CLASS rwmutex_t
    {
        public:
            class rguard_t
            {
                public:
                    inline rguard_t() { lock_=0; }
                    inline rguard_t(rwmutex_t &l) { lock_=0; lock(l); }
                    inline rguard_t(rwmutex_t *l) { lock_=0; lock(l); }
                    inline void unlock() { if(lock_) lock_->runlock(); lock_=0; }
                    inline void lock(rwmutex_t &l) { unlock(); l.rlock(); lock_=&l; }
                    inline void lock(rwmutex_t *l) { unlock(); l->rlock(); lock_=l; }
                    inline bool trylock(rwmutex_t &l) { unlock(); if(!l.tryrlock()) return false; lock_=&l; return true; }
                    inline bool trylock(rwmutex_t *l) { unlock(); if(!l->tryrlock()) return false; lock_=l; return true; }
                    inline ~rguard_t() { unlock(); }
                private:
                    rwmutex_t *lock_;
            };

            class wguard_t
            {
                public:
                    inline wguard_t() { lock_=0; }
                    inline wguard_t(rwmutex_t &l) { lock_=0; lock(l); }
                    inline wguard_t(rwmutex_t *l) { lock_=0; lock(l); }
                    inline void unlock() { if(lock_) lock_->wunlock(); lock_=0; }
                    inline void lock(rwmutex_t &l) { unlock(); l.wlock(); lock_=&l; }
                    inline void lock(rwmutex_t *l) { unlock(); l->wlock(); lock_=l; }
                    inline bool trylock(rwmutex_t &l) { unlock(); if(!l.trywlock()) return false; lock_=&l; return true; }
                    inline bool trylock(rwmutex_t *l) { unlock(); if(!l->trywlock()) return false; lock_=l; return true; }
                    inline ~wguard_t() { unlock(); }
                private:
                    rwmutex_t *lock_;
            };

        public:
            rwmutex_t();
            ~rwmutex_t();
            void rlock();
            void runlock();
            bool tryrlock();
            void wlock();
            void wunlock();
            bool trywlock();

        private:
            pthread_rwlock_t data_;
    };

    class PIC_DECLSPEC_CLASS mutex_t
    {
        public:
            class guard_t
            {
                public:
                    inline guard_t() { lock_=0; }
                    inline guard_t(mutex_t &l) { lock_=0; lock(l); }
                    inline guard_t(mutex_t *l) { lock_=0; lock(l); }
                    inline void unlock() { if(lock_) lock_->unlock(); lock_=0; }
                    inline void lock(mutex_t &l) { unlock(); l.lock(); lock_=&l; }
                    inline void lock(mutex_t *l) { unlock(); l->lock(); lock_=l; }
                    inline bool trylock(mutex_t &l) { unlock(); if(!l.trylock()) return false; lock_=&l; return true; }
                    inline bool trylock(mutex_t *l) { unlock(); if(!l->trylock()) return false; lock_=l; return true; }
                    inline ~guard_t() { unlock(); }
                private:
                    mutex_t *lock_;
            };

        public:
            mutex_t(bool recursive = false, bool inheritance = false);
            ~mutex_t();
            void lock();
            void unlock();
            bool trylock();

        private:
            pthread_mutex_t data_;
    };

    class PIC_DECLSPEC_CLASS gate_t
    {
        public:
            gate_t();
            ~gate_t();
            bool open();
            bool shut();
            void untimedpass();
            bool timedpass(unsigned long long timeout);
            bool isopen();
        private:
            pthread_cond_t c;
            pthread_mutex_t m;
            unsigned f;
    };

    class PIC_DECLSPEC_CLASS tsd_t
    {
        public:
            inline tsd_t() { pthread_key_create(&tsd_,0); }
            inline ~tsd_t() { pthread_key_delete(tsd_); }
            inline void *get() { return pthread_getspecific(tsd_); }
            inline void *set(void *x) { void *o=get(); pthread_setspecific(tsd_,x); return o; }
        private:
            pthread_key_t tsd_;
    };

    class PIC_DECLSPEC_CLASS thread_t
    {
        public:
            thread_t(int priority=PIC_THREAD_PRIORITY_NORMAL);
            virtual ~thread_t();
            void run();
            void wait();
            bool isrunning();

            static tsd_t genctx__;
            inline static void *tsd_setcontext(void *ctx) { return genctx__.set(ctx); }
            inline static void *tsd_getcontext() { return genctx__.get(); }

            virtual void thread_init() {}
            virtual void thread_main() {}
            virtual void thread_term() {}

        private:
            static void run__(thread_t *t);
            static void *run3__(void *t);
            bool run2__();

            void *context_;
            pic::logger_t *logger_;
            pic::nballocator_t *allocator_;
            gate_t init_gate_;
            bool init_;

            gate_t run_gate_;
            pic_threadid_t id_;
            int realtime_;
    };

    class PIC_DECLSPEC_CLASS xgate_t
    {
        public:
            xgate_t();
            void open();
            unsigned pass_and_shut_timed(unsigned long long t);
            unsigned pass_and_shut();
        private:
            pic_atomic_t flag_;
            pic::semaphore_t sem_;
    };

};

#endif
