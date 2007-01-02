<?php
/* Parse the rdup.rc file */
final class rc 
{
    var $compression;
    var $encryption;
    var $backup;
    var $directories;
    var $free;
    var $fifo;
    var $rdup;
    var $htpasswd;

    public function __construct($file) 
    {
        $this->compression = "";
        $this->encryption = "";
        $this->backuyp = "";
        $this->directories = "";
        $this->free = "";
        $this->fifo = "";
        $this->rdup = "";
        $this->htpasswd = "";
        $this->lockfile = "";
        $this->parse_rdup_rc($file);   
        return true;
    }

    function parse_rdup_rc($file)
    {
        global $backupdir;

        if (false === ($rc = file($file)))
            return true;
        foreach($rc as $l) {
            if (preg_match("/^#/", $l))
                continue;
            $l = rtrim($l, "\n");

            list($var, $value) = preg_split("/=/", $l);
            switch(strtoupper($var)) {
                case "DIRECTORIES":
                    $this->directories = $value;
                    break;
                case "BACKUP":
                    $this->backup = $value;
                    break;
                case "FREE":
                    $this->free = $value;
                    break;
                case "COMPRESSION":
                    $this->compression = $value;
                    break;
                case "ENCRYPTION":
                    $this->encryption = $value;
                    break;
                case "FIFO":
                    $this->fifo = $value;
                    break;
                case "RDUPSH":
                    $this->rdupsh = $value;
                    break;
                case "HTPASSWD":
                    $this->htpasswd = $value;
                    break;
                case "LOCKFILE":
                    $this->lockfile = $value;
            }
        }
        return true;
    }
}
?>
