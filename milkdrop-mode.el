; Milkdrop mode for Emacs
; Keywords generated from the Preset Authoring Guide, munged around a bit with
; sed, we highlight functions as builtins and variables as variables

; Only really provides font-lock, to make your milkdrop buffers look like angry
; fruit salad

; To interact with pmsh, please use inferior-pmsh-mode, which installs its own
; keys into the milkdrop-mode map

; FIXME:
;   * comments are not highlighted
;   * per-frame and per-pixel should be treated as keywords

(define-derived-mode milkdrop-mode
  conf-unix-mode "Milkdrop" "Milkdrop & ProjectM mode"
  (conf-mode-initialize "//")
  (setq font-lock-defaults
        (list
         (fold-left (function append) '() (generate-font-lock-table)))))

; Credit where credit's due:
; http://lambda.s55.xrea.com/Emacs.html
(defun fold-left (f b l)
  (if (null l) b (fold-left f (funcall f b (car l)) (cdr l))))

(defun generate-font-lock-table ()
  (mapcar
    (lambda (group)
      (mapcar
        (lambda (s)
          (milkdrop-identifier-keyword s (car group)))
        (cdr group)))
    milkdrop-font-lock))

(setq milkdrop-font-lock
  '((font-lock-builtin-face .
    ("sin" "int" "abs" "sin" "cos" "tan" "asin" "acos" "atan" "sqr" "sqrt"
      "pow" "log" "log10" "sign" "min" "max" "sigmoid" "rand" "bor" "bnot"
      "if" "equal" "above" "below"))
    (font-lock-variable-name-face . 
       ("zoom" "zoomexp" "rot" "warp" "cx" "cy" "dx" "dy" "sx" "sy" "wave_mode"
        "wave_x" "wave_y" "wave_r" "wave_g" "wave_b" "wave_a" "wave_mystery"
        "wave_usedots" "wave_thick" "wave_additive" "wave_brighten" "ob_size"
        "ob_r" "ob_g" "ob_b" "ob_a" "ib_size" "ib_r" "ib_g" "ib_b" "ib_a" "mv_r"
        "mv_g" "mv_b" "mv_a" "mv_x" "mv_y" "mv_l" "mv_dx" "mv_dy" "decay"
        "gamma" "echo_zoom" "echo_alpha" "echo_orient" "darken_center" "wrap"
        "invert" "brighten" "darken" "solarize" "monitor" "time" "fps" "frame"
        "progress" "bass" "mid" "treb" "bass_att" "mid_att" "treb_att" "meshx" 
        "meshy" "pixelsx" "pixelsy" "aspectx" "aspecty" "blur1_min" "blur2_min"
        "blur3_min" "blur1_max" "blur2_max" "blur3_max" "blur1_edge_darken"
        "q1" "q2" "q3" "q4" "q5" "q6" "q7" "q8" "q9" "q10" "q11" "q12" "q13"
        "q14" "q15" "q16" "q17" "q18" "q19" "q20" "q21" "q22" "q23" "q24"
        "q25" "q26" "q27" "q28" "q29" "q30" "q31" "q32" "x" "y" "rad" "ang"))))

(defun milkdrop-identifier-keyword (id face)
  (cons (milkdrop-identifier-regex id)
        (list 2 face)))

; This means: anything except an alphanumeric char or underscore, including
; nothing at beginning or end
(defun milkdrop-identifier-regex (id)
  (concat "\\([^_[:alnum:]]\\|\\`\\)"
          "\\("
          (regexp-quote id)
          "\\)"
          "\\([^_[:alnum:]]\\|\\'\\)"))

(provide 'milkdrop-mode)
