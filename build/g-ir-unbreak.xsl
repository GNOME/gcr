<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns="http://www.gtk.org/introspection/core/1.0"
    xmlns:gir="http://www.gtk.org/introspection/core/1.0"
    xmlns:c="http://www.gtk.org/introspection/c/1.0">

    <!--
      HACK: Workarounds for https://bugzilla.gnome.org/show_bug.cgi?id=725501
    -->
    <xsl:template match="gir:virtual-method[@name='get_der_data']">
        <xsl:copy>
            <xsl:attribute name="invoker">get_der_data</xsl:attribute>
            <xsl:apply-templates select="@*"/>
            <xsl:apply-templates select="../gir:method[@name='get_der_data']/*"/>
        </xsl:copy>
    </xsl:template>

    <xsl:template match="@*|node()">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:template>
</xsl:stylesheet>
