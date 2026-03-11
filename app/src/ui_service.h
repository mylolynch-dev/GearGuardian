/**
 * @file ui_service.h
 * @brief OLED UI service — semaphore-driven screen updates.
 *
 * ui_service runs as a low-priority thread.  Other modules signal it via
 * ui_service_request_update() whenever the display should be refreshed.
 * The thread snapshots g_app_state and calls the appropriate oled_screen_*()
 * function.
 */

#ifndef UI_SERVICE_H
#define UI_SERVICE_H

/**
 * @brief Signal the UI thread to redraw the display.
 *
 * ISR-safe (gives a semaphore).  Multiple signals before the thread wakes
 * collapse into one redraw.
 */
void ui_service_request_update(void);

#endif /* UI_SERVICE_H */
