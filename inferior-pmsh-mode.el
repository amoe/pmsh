; Flowchart:
; Emacs user presses C-x C-p r
; inferior-pmsh-mode inserts the string 'r' into *pmsh* buffer and sends it
; comint sends 'r' to pmsh process
; pmsh reads 'r' from stdin and does case analysis
; ProjectM reloads currently running preset

; Everything in weird style is copied from cmuscheme.el
; Some derived from inferior-io-mode.el and inferior-scala-mode.el

(require 'milkdrop-mode)
(require 'comint)

(defvar pmsh-program-name "/usr/local/bin/pmsh")

;; (defvar inferior-pmsh-mode-map
;;   (let ((m (copy-keymap comint-mode-map)))
;;     (define-key m (kbd "C-x C-r") 'pmsh-send-reload)
;;     m))

; Since we want to have bindings in both the milkdrop-mode map AND the pmsh mode
; map, it's best to write a function taking the map as a parameter and defining
; all the keys

    
;; (unless inferior-pmsh-mode-map
;;   (setq inferior-pmsh-mode-map (copy-keymap comint-mode-map))

;;   ; FIXME: this key should be defined in milkdrop-mode
;;   (define-key inferior-pmsh-mode-map (kbd "C-x C-k") 'pmsh-send-reload))

; Insert keybindings into milkdrop-mode
(define-derived-mode inferior-pmsh-mode comint-mode "Inferior pmsh"
  "Inferior pmsh interaction"
  (setq comint-prompt-regexp "^> *")
  (setq major-mode 'inferior-pmsh-mode)
  (setq mode-name "Inferior pmsh")
  (setq mode-line-process '(":%s"))
  (define-key inferior-pmsh-mode-map (kbd "C-c ; r") 'pmsh-send-reload))

(define-key milkdrop-mode-map (kbd "C-c ; r") 'pmsh-send-reload)
(define-key milkdrop-mode-map (kbd "C-c ; x") 'pmsh-send-exit)

(defun pmsh-proc ()
  (get-buffer-process "*pmsh*"))

; Remember to close buffer
(defun pmsh-send-exit ()
  (interactive)
  (comint-send-string (pmsh-proc) "x\n"))

(defun pmsh-send-reload ()
  (interactive)
  (comint-send-string (pmsh-proc) "r\n"))

(defun run-pmsh ()
  "Run inferior pmsh"
  (interactive)
  (make-comint-in-buffer
    "pmsh" "*pmsh*" pmsh-program-name)
  (pop-to-buffer "*pmsh*")
  (inferior-pmsh-mode))

(provide 'inferior-pmsh-mode)
