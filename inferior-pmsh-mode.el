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

; Insert keybindings into milkdrop-mode.  FIXME: bindings should be present in
; this mode as well
(define-derived-mode inferior-pmsh-mode comint-mode "Inferior pmsh"
  "Inferior pmsh interaction"
  (setq comint-prompt-regexp "^> *")
  (setq major-mode 'inferior-pmsh-mode)
  (setq mode-name "Inferior pmsh")
  (setq mode-line-process '(":%s"))
  (define-key inferior-pmsh-mode-map (kbd "C-c ; r") 'pmsh-send-reload))

(define-key milkdrop-mode-map (kbd "C-c ; r") 'pmsh-send-reload)
(define-key milkdrop-mode-map (kbd "C-c ; x") 'pmsh-send-exit)
(define-key milkdrop-mode-map (kbd "C-c ; l") 'pmsh-send-load)

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
