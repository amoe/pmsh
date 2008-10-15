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

(defvar pmsh-program-name "pmsh")

; Keymaps are passed by reference, so we take a keymap KM as input and add our
; bindings to it
(defun pmsh-bind-keys (km)
  (define-key km (kbd "C-c ; r") 'pmsh-send-reload)
  (define-key km (kbd "C-c ; x") 'pmsh-send-exit)
  (define-key km (kbd "C-c ; l") 'pmsh-send-load))

(define-derived-mode inferior-pmsh-mode comint-mode "Inferior pmsh"
  "Inferior pmsh interaction"
  (setq comint-prompt-regexp "^> *")
  (setq major-mode 'inferior-pmsh-mode)
  (setq mode-name "Inferior pmsh")
  (setq mode-line-process '(":%s"))
  (pmsh-bind-keys milkdrop-mode-map)
  (pmsh-bind-keys inferior-pmsh-mode-map))

(defun pmsh-proc ()
  (get-buffer-process "*pmsh*"))

; Remember to close buffer
(defun pmsh-send-exit ()
  (interactive)
  (comint-send-string (pmsh-proc) "x\n"))

(defun pmsh-send-reload ()
  (interactive)
  (comint-send-string (pmsh-proc) "r\n"))

(defun pmsh-send-load ()
  (interactive)
  (comint-send-string (pmsh-proc)
    (concat "l "
            (expand-file-name (read-file-name "Preset path: "))
            "\n")))

(defun run-pmsh ()
  "Run inferior pmsh"
  (interactive)
  (make-comint-in-buffer
    "pmsh" "*pmsh*" pmsh-program-name)
  (pop-to-buffer "*pmsh*")
  (inferior-pmsh-mode))

(provide 'inferior-pmsh-mode)
